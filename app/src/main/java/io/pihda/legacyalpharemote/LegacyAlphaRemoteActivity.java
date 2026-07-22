package io.pihda.legacyalpharemote;

import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.view.SurfaceView;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

public final class LegacyAlphaRemoteActivity
    extends BaseActivity implements SonyCameraController.Listener, WifiDirectController.Listener,
                                    AppNotificationManager.NotificationListener {
  private static final int HTTP_PORT = 8080;

  private Handler mainHandler;
  private TextView statusView;
  private ScrollView diagnosticsOverlay;
  private SonyCameraController cameraController;
  private WifiDirectController wifiController;
  private boolean resumed;
  private boolean nativeStarted;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    Logger.installUncaughtExceptionHandler();
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_legacy_alpha_remote);
    mainHandler = new Handler();
    statusView = (TextView) findViewById(R.id.remoteStatus);
    diagnosticsOverlay = (ScrollView) findViewById(R.id.diagnosticsOverlay);
    diagnosticsOverlay.setVisibility(View.VISIBLE);
    SurfaceView preview = (SurfaceView) findViewById(R.id.cameraPreview);
    cameraController = new SonyCameraController(preview, this);
    wifiController = new WifiDirectController(this, this);
    Logger.info("LegacyAlphaRemote: created version=0.1.0 native=" + NativeBridge.nativeVersion());
  }

  @Override
  protected void onResume() {
    super.onResume();
    resumed = true;
    AppNotificationManager.getInstance().addListener(this);
    Logger.info("LegacyAlphaRemote: onResume model=" + Build.MODEL + " product=" + Build.PRODUCT
        + " display=" + Build.DISPLAY + " firmware=" + Build.VERSION.INCREMENTAL);
    try {
      setAutoPowerOffMode(false);
      Logger.info("LegacyAlphaRemote: automatic power-off disabled");
    } catch (RuntimeException e) {
      Logger.error("LegacyAlphaRemote: APO disable failed " + e.toString());
    }
    cameraController.start();
    nativeStarted = NativeBridge.nativeStart(this, Logger.getFile().getAbsolutePath(), HTTP_PORT);
    if (!nativeStarted) {
      setStatus("Legacy Alpha Remote\n\nNative runtime failed to start");
      Logger.error("LegacyAlphaRemote: native runtime failed to start");
    } else {
      NativeBridge.nativeSetCameraReady(cameraController.isReady());
      wifiController.start();
    }
  }

  @Override
  protected void onPause() {
    Logger.info("LegacyAlphaRemote: onPause begin");
    resumed = false;
    AppNotificationManager.getInstance().removeListener(this);
    if (nativeStarted) {
      NativeBridge.nativeStop();
      nativeStarted = false;
    }
    wifiController.stop();
    cameraController.stop();
    try {
      setAutoPowerOffMode(true);
      Logger.info("LegacyAlphaRemote: automatic power-off restored");
    } catch (RuntimeException e) {
      Logger.error("LegacyAlphaRemote: APO restore failed " + e.toString());
    }
    super.onPause();
    Logger.info("LegacyAlphaRemote: onPause complete");
  }

  public void dispatchNativeCommand(final long commandId, final int commandType,
      final int intArgument, final String stringArgument) {
    mainHandler.post(new Runnable() {
      @Override
      public void run() {
        if (commandType == NativeBridge.COMMAND_UPDATE_SCREEN) {
          if (resumed)
            setStatus(stringArgument == null ? "" : stringArgument);
          return;
        }
        if (!resumed) {
          NativeBridge.nativeReportCommandResult(commandId, commandType, -1, "activity paused");
          return;
        }
        cameraController.executeCommand(commandId, commandType);
      }
    });
  }

  @Override
  public void onCameraReadyChanged(boolean ready) {
    if (nativeStarted)
      NativeBridge.nativeSetCameraReady(ready);
  }

  @Override
  public void onCameraCommandResult(
      long commandId, int commandType, int resultCode, String message) {
    if (nativeStarted) {
      NativeBridge.nativeReportCommandResult(commandId, commandType, resultCode, message);
    }
  }

  @Override
  public void onCameraError(String message) {
    if (!nativeStarted)
      setStatus("Legacy Alpha Remote\n\n" + message);
  }

  @Override
  public void onCameraPreview(byte[] jpeg, int width, int height, String source, String error) {
    if (nativeStarted) {
      NativeBridge.nativeSetPreview(jpeg, width, height, source, error);
    }
  }

  @Override
  public void onWifiInfo(
      boolean ready, String ssid, String password, String address, int stationCount, String error) {
    if (nativeStarted) {
      NativeBridge.nativeSetWifiState(ready, ssid, password, address, stationCount, error);
    } else if (error != null && error.length() > 0) {
      setStatus("Legacy Alpha Remote\n\n" + error);
    }
  }

  @Override
  public void onNotify(String message) {
    Logger.info("LegacyAlphaRemote: display changed");
  }

  @Override
  protected boolean onFocusKeyDown() {
    return nativeStarted && NativeBridge.nativePhysicalFocus(true);
  }

  @Override
  protected boolean onFocusKeyUp() {
    return nativeStarted && NativeBridge.nativePhysicalFocus(false);
  }

  @Override
  protected boolean onShutterKeyDown() {
    return nativeStarted && NativeBridge.nativePhysicalShutter(true);
  }

  @Override
  protected boolean onShutterKeyUp() {
    return nativeStarted && NativeBridge.nativePhysicalShutter(false);
  }

  @Override
  protected boolean onDeleteKeyUp() {
    finish();
    return true;
  }

  @Override
  protected boolean onUpKeyUp() {
    boolean visible = diagnosticsOverlay.getVisibility() == View.VISIBLE;
    diagnosticsOverlay.setVisibility(visible ? View.GONE : View.VISIBLE);
    Logger.info("LegacyAlphaRemote: diagnostics overlay " + (visible ? "hidden" : "shown"));
    return true;
  }

  @Override
  protected void setColorDepth(boolean highQuality) {
    super.setColorDepth(false);
  }

  private void setStatus(String status) {
    statusView.setText(status);
  }
}
