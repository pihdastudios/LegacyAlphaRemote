package io.pihda.legacyalpharemote;

public final class NativeBridge {
  public static final int COMMAND_START_AUTOFOCUS = 1;
  public static final int COMMAND_CANCEL_AUTOFOCUS = 2;
  public static final int COMMAND_CAPTURE = 3;
  public static final int COMMAND_RELEASE_SHUTTER = 4;
  public static final int COMMAND_RESTART_PREVIEW = 5;
  public static final int COMMAND_UPDATE_SCREEN = 6;

  static {
    System.loadLibrary("legacyalpharemote");
    Logger.info("NativeBridge: liblegacyalpharemote loaded");
  }

  private NativeBridge() {}

  public static native String nativeVersion();
  public static native boolean nativeStart(Object activity, String logPath, int port);
  public static native void nativeStop();
  public static native void nativeSetCameraReady(boolean ready);
  public static native void nativeSetWifiState(
      boolean ready, String ssid, String password, String address, int stationCount, String error);
  public static native void nativeSetPreview(
      byte[] jpeg, int width, int height, String source, String error);
  public static native void nativeReportCommandResult(
      long commandId, int commandType, int resultCode, String message);
  public static native boolean nativePhysicalFocus(boolean pressed);
  public static native boolean nativePhysicalShutter(boolean pressed);
}
