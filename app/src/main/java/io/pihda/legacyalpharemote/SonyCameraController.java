package io.pihda.legacyalpharemote;

import android.hardware.Camera;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.sony.scalar.hardware.CameraEx;

import java.io.IOException;

public final class SonyCameraController implements SurfaceHolder.Callback {
    public interface Listener {
        void onCameraReadyChanged(boolean ready);
        void onCameraCommandResult(long commandId, int commandType,
                int resultCode, String message);
        void onCameraError(String message);
    }

    private final SurfaceHolder surfaceHolder;
    private final Listener listener;
    private CameraEx cameraEx;
    private boolean active;
    private boolean surfaceAvailable;
    private boolean previewStarted;

    public SonyCameraController(SurfaceView surfaceView, Listener listener) {
        this.listener = listener;
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
    }

    public void start() {
        if (active) return;
        active = true;
        previewStarted = false;
        surfaceHolder.addCallback(this);
        try {
            cameraEx = CameraEx.open(0, null);
            Logger.info("Camera: CameraEx.open completed");
            if (surfaceAvailable) startPreview();
        } catch (RuntimeException e) {
            cameraEx = null;
            reportError("Camera open failed", e);
        }
    }

    public void stop() {
        if (!active && cameraEx == null) return;
        active = false;
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
                cameraEx.release();
                Logger.info("Camera: CameraEx released");
            } catch (RuntimeException e) {
                reportError("Camera release failed", e);
            }
            cameraEx = null;
        }
        previewStarted = false;
        surfaceHolder.removeCallback(this);
    }

    public boolean isReady() {
        return active && cameraEx != null && previewStarted;
    }

    public void executeCommand(long commandId, int commandType) {
        if (cameraEx == null || !active) {
            listener.onCameraCommandResult(commandId, commandType, -1,
                    "camera unavailable");
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
                    Logger.info("Camera: capture request accepted");
                    break;
                case NativeBridge.COMMAND_RELEASE_SHUTTER:
                    cameraEx.cancelTakePicture();
                    Logger.info("Camera: shutter released");
                    break;
                case NativeBridge.COMMAND_RESTART_PREVIEW:
                    restartPreviewIfRequired();
                    break;
                default:
                    listener.onCameraCommandResult(commandId, commandType, -2,
                            "unknown camera command");
                    return;
            }
            listener.onCameraCommandResult(commandId, commandType, 0, "ok");
        } catch (RuntimeException e) {
            reportError("Camera command failed type=" + commandType, e);
            listener.onCameraCommandResult(commandId, commandType, -1,
                    e.getClass().getName());
        }
    }

    private void startPreview() {
        if (!active || cameraEx == null || !surfaceAvailable || previewStarted) return;
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
        if (previewStarted) return;
        startPreview();
        if (!previewStarted) throw new IllegalStateException("preview unavailable");
        Logger.info("Camera: normal preview restarted");
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
}
