package io.pihda.legacyalpharemote;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.os.Handler;
import android.os.SystemClock;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import com.sony.scalar.hardware.CameraEx;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public final class SonyCameraController implements SurfaceHolder.Callback {
  private static final int PREVIEW_MAX_DIMENSION = 1280;
  private static final int PREVIEW_JPEG_QUALITY = 80;
  private static final int PREVIEW_MAX_BYTES = 1536 * 1024;
  private static final long JPEG_TIMEOUT_MS = 5000L;

  public interface Listener {
    void onCameraReadyChanged(boolean ready);
    void onCameraCommandResult(long commandId, int commandType, int resultCode, String message);
    void onCameraError(String message);
    void onCameraPreview(byte[] jpeg, int width, int height, String source, String error);
  }

  private final SurfaceHolder surfaceHolder;
  private final Listener listener;
  private final Handler mainHandler = new Handler();
  private CameraEx cameraEx;
  private ThreadPoolExecutor previewWorker;
  private boolean active;
  private boolean surfaceAvailable;
  private boolean previewStarted;
  private int captureGeneration;
  private int fallbackGeneration;
  private long fallbackDeadlineMs;

  public SonyCameraController(SurfaceView surfaceView, Listener listener) {
    this.listener = listener;
    surfaceHolder = surfaceView.getHolder();
    surfaceHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
  }

  public void start() {
    if (active)
      return;
    active = true;
    previewStarted = false;
    previewWorker = new ThreadPoolExecutor(
        1, 1, 0L, TimeUnit.MILLISECONDS, new ArrayBlockingQueue<Runnable>(1));
    surfaceHolder.addCallback(this);
    try {
      cameraEx = CameraEx.open(0, null);
      cameraEx.setJpegListener(new CameraEx.JpegListener() {
        @Override
        public void onPictureTaken(byte[] data, CameraEx camera) {
          handleCapturedJpeg(data);
        }
      });
      Logger.info("Camera: CameraEx.open completed");
      if (surfaceAvailable)
        startPreview();
    } catch (RuntimeException e) {
      cameraEx = null;
      reportError("Camera open failed", e);
    }
  }

  public void stop() {
    if (!active && cameraEx == null)
      return;
    active = false;
    captureGeneration++;
    fallbackGeneration = 0;
    fallbackDeadlineMs = 0L;
    mainHandler.removeCallbacksAndMessages(null);
    listener.onCameraReadyChanged(false);
    if (cameraEx != null) {
      try {
        cameraEx.getNormalCamera().cancelAutoFocus();
      } catch (RuntimeException e) {
        reportError("Cancel autofocus during release failed", e);
      }
      try {
        cameraEx.cancelTakePicture();
      } catch (RuntimeException e) {
        reportError("Cancel shutter during release failed", e);
      }
      try {
        cameraEx.setJpegListener(null);
      } catch (RuntimeException e) {
        reportError("Remove JPEG listener failed", e);
      }
      try {
        cameraEx.release();
        Logger.info("Camera: CameraEx released");
      } catch (RuntimeException e) {
        reportError("Camera release failed", e);
      }
      cameraEx = null;
    }
    previewStarted = false;
    if (previewWorker != null) {
      previewWorker.shutdownNow();
      previewWorker = null;
    }
    surfaceHolder.removeCallback(this);
  }

  public boolean isReady() {
    return active && cameraEx != null && previewStarted;
  }

  public void executeCommand(long commandId, int commandType) {
    if (cameraEx == null || !active) {
      listener.onCameraCommandResult(commandId, commandType, -1, "camera unavailable");
      return;
    }
    try {
      switch (commandType) {
        case NativeBridge.COMMAND_START_AUTOFOCUS:
          cameraEx.getNormalCamera().autoFocus(null);
          Logger.info("Camera: autofocus requested");
          break;
        case NativeBridge.COMMAND_CANCEL_AUTOFOCUS:
          cameraEx.getNormalCamera().cancelAutoFocus();
          Logger.info("Camera: autofocus cancelled");
          break;
        case NativeBridge.COMMAND_CAPTURE:
          cameraEx.getNormalCamera().takePicture(null, null, null);
          previewStarted = false;
          scheduleFallback(++captureGeneration);
          Logger.info("Camera: capture request accepted");
          break;
        case NativeBridge.COMMAND_RELEASE_SHUTTER:
          cameraEx.cancelTakePicture();
          Logger.info("Camera: shutter released");
          restartPreviewIfRequired();
          break;
        case NativeBridge.COMMAND_RESTART_PREVIEW:
          restartPreviewIfRequired();
          break;
        default:
          listener.onCameraCommandResult(commandId, commandType, -2, "unknown camera command");
          return;
      }
      listener.onCameraCommandResult(commandId, commandType, 0, "ok");
    } catch (RuntimeException e) {
      reportError("Camera command failed type=" + commandType, e);
      listener.onCameraCommandResult(commandId, commandType, -1, e.getClass().getName());
    }
  }

  private void startPreview() {
    if (!active || cameraEx == null || !surfaceAvailable || previewStarted)
      return;
    try {
      Camera camera = cameraEx.getNormalCamera();
      camera.setPreviewDisplay(surfaceHolder);
      camera.startPreview();
      previewStarted = true;
      Logger.info("Camera: normal preview started");
      listener.onCameraReadyChanged(true);
    } catch (IOException e) {
      reportError("Preview display failed", e);
    } catch (RuntimeException e) {
      reportError("Preview start failed", e);
    }
  }

  private void restartPreviewIfRequired() {
    if (previewStarted)
      return;
    startPreview();
    if (!previewStarted)
      throw new IllegalStateException("preview unavailable");
    Logger.info("Camera: normal preview restarted");
    requestPendingFallback();
  }

  @Override
  public void surfaceCreated(SurfaceHolder holder) {
    surfaceAvailable = true;
    Logger.info("Camera: surface created");
    startPreview();
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    Logger.info("Camera: surface changed width=" + width + " height=" + height);
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {
    surfaceAvailable = false;
    previewStarted = false;
    listener.onCameraReadyChanged(false);
    Logger.info("Camera: surface destroyed");
  }

  private void reportError(String operation, Throwable throwable) {
    String message = operation + ": " + throwable.toString();
    Logger.error(message);
    listener.onCameraError(message);
  }

  private void scheduleFallback(final int generation) {
    fallbackGeneration = generation;
    fallbackDeadlineMs = SystemClock.uptimeMillis() + JPEG_TIMEOUT_MS;
    mainHandler.postDelayed(new Runnable() {
      @Override
      public void run() {
        if (!active || fallbackGeneration != generation)
          return;
        if (previewStarted)
          requestLiveViewFallback(generation);
      }
    }, JPEG_TIMEOUT_MS);
  }

  private void requestPendingFallback() {
    final int generation = fallbackGeneration;
    if (generation == 0 || generation != captureGeneration)
      return;
    long delay = fallbackDeadlineMs - SystemClock.uptimeMillis();
    if (delay < 0L)
      delay = 0L;
    mainHandler.postDelayed(new Runnable() {
      @Override
      public void run() {
        if (active && previewStarted && fallbackGeneration == generation) {
          requestLiveViewFallback(generation);
        }
      }
    }, delay);
  }

  private void requestLiveViewFallback(final int generation) {
    if (cameraEx == null || !previewStarted)
      return;
    try {
      cameraEx.getNormalCamera().setOneShotPreviewCallback(new Camera.PreviewCallback() {
        @Override
        public void onPreviewFrame(byte[] data, Camera camera) {
          if (fallbackGeneration != generation)
            return;
          fallbackGeneration = 0;
          fallbackDeadlineMs = 0L;
          Camera.Size size = camera.getParameters().getPreviewSize();
          int format = camera.getParameters().getPreviewFormat();
          queueLiveView((byte[]) data.clone(), size.width, size.height, format);
        }
      });
    } catch (RuntimeException e) {
      fallbackGeneration = 0;
      fallbackDeadlineMs = 0L;
      publishPreviewError("live_view_fallback", "Fallback request failed: " + e.toString());
    }
  }

  private void handleCapturedJpeg(byte[] data) {
    if (!active)
      return;
    fallbackGeneration = 0;
    fallbackDeadlineMs = 0L;
    if (data == null || data.length == 0) {
      publishPreviewError("captured_jpeg", "Camera returned an empty JPEG");
      return;
    }
    queueJpeg((byte[]) data.clone(), "captured_jpeg");
  }

  private void queueLiveView(
      final byte[] data, final int width, final int height, final int format) {
    submitPreviewTask(new Runnable() {
      @Override
      public void run() {
        try {
          if (format != ImageFormat.NV21) {
            throw new IOException("Unsupported preview format " + format);
          }
          ByteArrayOutputStream output = new ByteArrayOutputStream();
          YuvImage image = new YuvImage(data, format, width, height, null);
          if (!image.compressToJpeg(new Rect(0, 0, width, height), PREVIEW_JPEG_QUALITY, output)) {
            throw new IOException("Preview JPEG conversion failed");
          }
          processJpeg(output.toByteArray(), "live_view_fallback");
        } catch (Throwable e) {
          publishPreviewError("live_view_fallback", e.toString());
        }
      }
    }, "live_view_fallback");
  }

  private void queueJpeg(final byte[] data, final String source) {
    submitPreviewTask(new Runnable() {
      @Override
      public void run() {
        processJpeg(data, source);
      }
    }, source);
  }

  private void submitPreviewTask(Runnable task, String source) {
    ThreadPoolExecutor worker = previewWorker;
    if (worker == null || worker.isShutdown())
      return;
    try {
      worker.execute(task);
    } catch (RejectedExecutionException e) {
      publishPreviewError(source, "Preview worker busy");
    }
  }

  private void processJpeg(byte[] data, String source) {
    Bitmap decoded = null;
    Bitmap scaled = null;
    try {
      BitmapFactory.Options bounds = new BitmapFactory.Options();
      bounds.inJustDecodeBounds = true;
      BitmapFactory.decodeByteArray(data, 0, data.length, bounds);
      if (bounds.outWidth <= 0 || bounds.outHeight <= 0) {
        throw new IOException("Invalid JPEG dimensions");
      }
      int sample = 1;
      while (bounds.outWidth / sample > PREVIEW_MAX_DIMENSION * 2
          || bounds.outHeight / sample > PREVIEW_MAX_DIMENSION * 2) {
        sample *= 2;
      }
      BitmapFactory.Options options = new BitmapFactory.Options();
      options.inSampleSize = sample;
      decoded = BitmapFactory.decodeByteArray(data, 0, data.length, options);
      if (decoded == null)
        throw new IOException("JPEG decode failed");

      int width = decoded.getWidth();
      int height = decoded.getHeight();
      int longest = Math.max(width, height);
      Bitmap outputBitmap = decoded;
      if (longest > PREVIEW_MAX_DIMENSION) {
        float scale = (float) PREVIEW_MAX_DIMENSION / (float) longest;
        width = Math.max(1, Math.round(width * scale));
        height = Math.max(1, Math.round(height * scale));
        scaled = Bitmap.createScaledBitmap(decoded, width, height, true);
        outputBitmap = scaled;
      }
      ByteArrayOutputStream output = new ByteArrayOutputStream();
      if (!outputBitmap.compress(Bitmap.CompressFormat.JPEG, PREVIEW_JPEG_QUALITY, output)) {
        throw new IOException("JPEG encode failed");
      }
      byte[] jpeg = output.toByteArray();
      if (jpeg.length > PREVIEW_MAX_BYTES) {
        throw new IOException("Preview exceeds " + PREVIEW_MAX_BYTES + " bytes");
      }
      listener.onCameraPreview(jpeg, width, height, source, "");
      Logger.info("Camera: preview published source=" + source + " width=" + width
          + " height=" + height + " bytes=" + jpeg.length);
    } catch (Throwable e) {
      publishPreviewError(source, e.toString());
    } finally {
      if (scaled != null)
        scaled.recycle();
      if (decoded != null)
        decoded.recycle();
    }
  }

  private void publishPreviewError(String source, String error) {
    Logger.error("Camera: preview error source=" + source + " " + error);
    listener.onCameraPreview(null, 0, 0, source, error);
  }
}
