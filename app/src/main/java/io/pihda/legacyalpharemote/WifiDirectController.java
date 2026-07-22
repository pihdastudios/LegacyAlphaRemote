package io.pihda.legacyalpharemote;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.WifiManager;
import com.sony.wifi.direct.DirectConfiguration;
import com.sony.wifi.direct.DirectManager;
import java.util.List;

public final class WifiDirectController {
  public interface Listener {
    void onWifiInfo(boolean ready, String ssid, String password, String address, int stationCount,
        String error);
  }

  private static final String DEFAULT_ADDRESS = "192.168.122.1";

  private final Context context;
  private final Listener listener;
  private final WifiManager wifiManager;
  private final DirectManager directManager;
  private final BroadcastReceiver wifiStateReceiver;
  private final BroadcastReceiver directStateReceiver;
  private final BroadcastReceiver groupSuccessReceiver;
  private final BroadcastReceiver groupFailureReceiver;
  private final BroadcastReceiver stationConnectedReceiver;
  private final BroadcastReceiver stationDisconnectedReceiver;

  private boolean started;
  private boolean receiversRegistered;
  private boolean wifiOriginallyEnabled;
  private boolean directOriginallyEnabled;
  private boolean groupStarted;
  private String ssid = "";
  private String password = "";
  private String address = DEFAULT_ADDRESS;
  private int stationCount;

  public WifiDirectController(Context context, Listener listener) {
    this.context = context;
    this.listener = listener;
    wifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
    directManager = (DirectManager) context.getSystemService(DirectManager.WIFI_DIRECT_SERVICE);

    wifiStateReceiver = new BroadcastReceiver() {
      @Override
      public void onReceive(Context receiverContext, Intent intent) {
        int state =
            intent.getIntExtra(WifiManager.EXTRA_WIFI_STATE, WifiManager.WIFI_STATE_UNKNOWN);
        wifiStateChanged(state);
      }
    };
    directStateReceiver = new BroadcastReceiver() {
      @Override
      public void onReceive(Context receiverContext, Intent intent) {
        int state = intent.getIntExtra(
            DirectManager.EXTRA_DIRECT_STATE, DirectManager.DIRECT_STATE_UNKNOWN);
        directStateChanged(state);
      }
    };
    groupSuccessReceiver = new BroadcastReceiver() {
      @Override
      public void onReceive(Context receiverContext, Intent intent) {
        DirectConfiguration configuration =
            (DirectConfiguration) intent.getParcelableExtra(DirectManager.EXTRA_DIRECT_CONFIG);
        groupCreated(configuration);
      }
    };
    groupFailureReceiver = new BroadcastReceiver() {
      @Override
      public void onReceive(Context receiverContext, Intent intent) {
        fail("Wi-Fi Direct group creation failed");
      }
    };
    stationConnectedReceiver = new BroadcastReceiver() {
      @Override
      public void onReceive(Context receiverContext, Intent intent) {
        refreshStationCount();
        Logger.info("WiFiDirect: station connected count=" + stationCount);
        publish(true, "");
      }
    };
    stationDisconnectedReceiver = new BroadcastReceiver() {
      @Override
      public void onReceive(Context receiverContext, Intent intent) {
        refreshStationCount();
        Logger.info("WiFiDirect: station disconnected count=" + stationCount);
        publish(groupStarted, "");
      }
    };
  }

  public void start() {
    if (started)
      return;
    started = true;
    groupStarted = false;
    stationCount = 0;
    if (wifiManager == null || directManager == null) {
      fail("Sony Wi-Fi Direct service unavailable");
      return;
    }
    wifiOriginallyEnabled = wifiManager.isWifiEnabled();
    directOriginallyEnabled = directManager.isDirectEnabled();
    registerReceivers();
    Logger.info("WiFiDirect: start wifiOriginallyEnabled=" + wifiOriginallyEnabled
        + " directOriginallyEnabled=" + directOriginallyEnabled);
    if (!wifiOriginallyEnabled && !wifiManager.setWifiEnabled(true)) {
      fail("Unable to enable Wi-Fi");
      return;
    }
    if (wifiOriginallyEnabled)
      enableDirect();
  }

  public void stop() {
    if (!started && !receiversRegistered)
      return;
    started = false;
    publish(false, "stopped");
    if (directManager != null) {
      if (groupStarted) {
        try {
          directManager.removeGroup();
          Logger.info("WiFiDirect: removeGroup requested");
        } catch (RuntimeException e) {
          Logger.error("WiFiDirect: removeGroup failed " + e.toString());
        }
      }
      if (!directOriginallyEnabled) {
        try {
          directManager.setDirectEnabled(false);
          Logger.info("WiFiDirect: disabled");
        } catch (RuntimeException e) {
          Logger.error("WiFiDirect: disable failed " + e.toString());
        }
      }
    }
    if (wifiManager != null && !wifiOriginallyEnabled) {
      try {
        wifiManager.setWifiEnabled(false);
        Logger.info("WiFiDirect: Wi-Fi disabled");
      } catch (RuntimeException e) {
        Logger.error("WiFiDirect: Wi-Fi disable failed " + e.toString());
      }
    }
    unregisterReceivers();
    groupStarted = false;
    stationCount = 0;
  }

  private void registerReceivers() {
    if (receiversRegistered)
      return;
    context.registerReceiver(
        wifiStateReceiver, new IntentFilter(WifiManager.WIFI_STATE_CHANGED_ACTION));
    context.registerReceiver(
        directStateReceiver, new IntentFilter(DirectManager.DIRECT_STATE_CHANGED_ACTION));
    context.registerReceiver(
        groupSuccessReceiver, new IntentFilter(DirectManager.GROUP_CREATE_SUCCESS_ACTION));
    context.registerReceiver(
        groupFailureReceiver, new IntentFilter(DirectManager.GROUP_CREATE_FAILURE_ACTION));
    context.registerReceiver(
        stationConnectedReceiver, new IntentFilter(DirectManager.STA_CONNECTED_ACTION));
    context.registerReceiver(
        stationDisconnectedReceiver, new IntentFilter(DirectManager.STA_DISCONNECTED_ACTION));
    receiversRegistered = true;
    Logger.info("WiFiDirect: receivers registered");
  }

  private void unregisterReceivers() {
    if (!receiversRegistered)
      return;
    safeUnregister(wifiStateReceiver);
    safeUnregister(directStateReceiver);
    safeUnregister(groupSuccessReceiver);
    safeUnregister(groupFailureReceiver);
    safeUnregister(stationConnectedReceiver);
    safeUnregister(stationDisconnectedReceiver);
    receiversRegistered = false;
    Logger.info("WiFiDirect: receivers unregistered");
  }

  private void safeUnregister(BroadcastReceiver receiver) {
    try {
      context.unregisterReceiver(receiver);
    } catch (IllegalArgumentException e) {
      Logger.error("WiFiDirect: receiver was not registered " + e.toString());
    }
  }

  private void wifiStateChanged(int state) {
    Logger.info("WiFiDirect: Wi-Fi state=" + state);
    if (started && state == WifiManager.WIFI_STATE_ENABLED)
      enableDirect();
  }

  private void enableDirect() {
    if (!started)
      return;
    try {
      if (directManager.isDirectEnabled()) {
        createGroup();
      } else if (!directManager.setDirectEnabled(true)) {
        fail("Unable to enable Sony Wi-Fi Direct");
      } else {
        Logger.info("WiFiDirect: enable requested");
      }
    } catch (RuntimeException e) {
      fail("Wi-Fi Direct enable failed: " + e.getClass().getName());
    }
  }

  private void directStateChanged(int state) {
    Logger.info("WiFiDirect: direct state=" + state);
    if (started && state == DirectManager.DIRECT_STATE_ENABLED)
      createGroup();
  }

  private void createGroup() {
    if (!started || groupStarted)
      return;
    try {
      List configurations = directManager.getConfigurations();
      if (configurations == null || configurations.isEmpty()) {
        fail("No saved Sony Wi-Fi Direct configuration");
        return;
      }
      DirectConfiguration configuration =
          (DirectConfiguration) configurations.get(configurations.size() - 1);
      Logger.info("WiFiDirect: startGo networkId=" + configuration.getNetworkId());
      if (!directManager.startGo(configuration.getNetworkId())) {
        fail("Sony startGo rejected group creation");
      }
    } catch (RuntimeException e) {
      fail("Wi-Fi Direct group request failed: " + e.getClass().getName());
    }
  }

  private void groupCreated(DirectConfiguration configuration) {
    if (!started || configuration == null) {
      fail("Wi-Fi Direct returned no group configuration");
      return;
    }
    groupStarted = true;
    ssid = valueOrEmpty(configuration.getSsid());
    password = valueOrEmpty(configuration.getPreSharedKey());
    address = normalizeAddress(configuration.getP2PIfAddress());
    refreshStationCount();
    Logger.info("WiFiDirect: group created ssid=" + ssid
        + " interface=" + valueOrEmpty(configuration.getIfName()) + " address=" + address);
    publish(true, "");
  }

  private void refreshStationCount() {
    try {
      List stations = directManager.getStationList();
      stationCount = stations == null ? 0 : stations.size();
    } catch (RuntimeException e) {
      stationCount = 0;
      Logger.error("WiFiDirect: station list failed " + e.toString());
    }
  }

  private String normalizeAddress(String candidate) {
    if (candidate == null || candidate.length() == 0)
      return DEFAULT_ADDRESS;
    if (candidate.charAt(0) == '/')
      candidate = candidate.substring(1);
    int slash = candidate.indexOf('/');
    if (slash >= 0)
      candidate = candidate.substring(0, slash);
    return isIpv4Address(candidate) ? candidate : DEFAULT_ADDRESS;
  }

  private boolean isIpv4Address(String candidate) {
    String[] parts = candidate == null ? new String[0] : candidate.split("\\.", -1);
    if (parts.length != 4)
      return false;
    for (int i = 0; i < parts.length; ++i) {
      if (parts[i].length() == 0 || parts[i].length() > 3)
        return false;
      int value = 0;
      for (int j = 0; j < parts[i].length(); ++j) {
        char digit = parts[i].charAt(j);
        if (digit < '0' || digit > '9')
          return false;
        value = value * 10 + digit - '0';
      }
      if (value > 255)
        return false;
    }
    return true;
  }

  private String valueOrEmpty(String value) {
    return value == null ? "" : value;
  }

  private void publish(boolean ready, String error) {
    listener.onWifiInfo(ready, ssid, password, address, stationCount, error);
  }

  private void fail(String message) {
    Logger.error("WiFiDirect: " + message);
    publish(false, message);
  }
}
