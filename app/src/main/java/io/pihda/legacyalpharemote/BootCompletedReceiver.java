package io.pihda.legacyalpharemote;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class BootCompletedReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        Logger.info("LegacyAlphaRemote: camera boot completed");
    }
}
