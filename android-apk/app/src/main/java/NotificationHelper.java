package com.sega.sonicunr;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.BitmapFactory;

public class NotificationHelper {

    private static final String CHANNEL_ID = "icon_switch_channel";
    private static final String CHANNEL_NAME = "Icon Switch Alerts";
    public static final int NOTIFICATION_ID = 1001;

    public static void createChannelIfNeeded(Context context) {
        NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null) return;

        if (manager.getNotificationChannel(CHANNEL_ID) == null) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    CHANNEL_NAME,
                    NotificationManager.IMPORTANCE_DEFAULT
            );
            channel.setDescription("Notifications about day/night icon switching");
            manager.createNotificationChannel(channel);
        }
    }

    public static void showOrUpdate(Context context, String contentText, int smallIconResId, int largeIconResId) {
        createChannelIfNeeded(context);

        Notification notification = new Notification.Builder(context, CHANNEL_ID)
                .setContentTitle("Icon Switch")
                .setContentText(contentText)
                .setSmallIcon(smallIconResId)
                .setLargeIcon(BitmapFactory.decodeResource(context.getResources(), largeIconResId))
                .setPriority(Notification.PRIORITY_DEFAULT)
                .setOnlyAlertOnce(true) // não vibra/soa de novo ao atualizar
                .setAutoCancel(false)
                .build();

        NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.notify(NOTIFICATION_ID, notification);
        }
    }

    public static void cancel(Context context) {
        NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager != null) {
            manager.cancel(NOTIFICATION_ID);
        }
    }
}