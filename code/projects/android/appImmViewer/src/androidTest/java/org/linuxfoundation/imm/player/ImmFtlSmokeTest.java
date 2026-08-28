package org.linuxfoundation.imm.player;

import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

@RunWith(AndroidJUnit4.class)
public final class ImmFtlSmokeTest {
    private static final String PACKAGE_NAME = "org.linuxfoundation.imm.player";
    private static final int VALIDATION_RENDER_WIDTH = 1280;
    private static final int VALIDATION_RENDER_HEIGHT = 720;
    private static final double DEFAULT_VALIDATION_FIXED_DT = 0.0333333333333333;
    private static final long DEFAULT_VALIDATION_PLAYER_FRAME = 60L;

    @Test
    public void rendersSampleFrame() throws Exception {
        Bundle args = InstrumentationRegistry.getArguments();
        String renderer = args.getString("renderer", "Vulkan");
        int waitSeconds = parseInt(args.getString("waitSeconds"), 25);
        double validationFixedDt = parseDouble(args.getString("ValidationFixedDt"), DEFAULT_VALIDATION_FIXED_DT);
        long validationPlayerFrame = parseLong(args.getString("ValidationPlayerFrame"), DEFAULT_VALIDATION_PLAYER_FRAME);

        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        Context targetContext = InstrumentationRegistry.getInstrumentation().getTargetContext();

        runShell(device, "logcat -c");

        Intent intent = targetContext.getPackageManager().getLaunchIntentForPackage(PACKAGE_NAME);
        assertTrue("Launch intent not found for " + PACKAGE_NAME, intent != null);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        intent.putExtra("RenderingAPI", renderer);
        intent.putExtra("ValidationRenderWidth", VALIDATION_RENDER_WIDTH);
        intent.putExtra("ValidationRenderHeight", VALIDATION_RENDER_HEIGHT);
        intent.putExtra("ValidationFixedDt", validationFixedDt);
        intent.putExtra("ValidationPlayerFrame", validationPlayerFrame);
        targetContext.startActivity(intent);

        File artifactDir = new File(targetContext.getExternalFilesDir(null), "imm-ftl");
        assertTrue("Could not create artifact directory: " + artifactDir, artifactDir.mkdirs() || artifactDir.isDirectory());

        File nativeCapture = new File(artifactDir, "native-render-after.ppm");
        String logcat = waitForNativeCapture(device, nativeCapture, waitSeconds);

        File screenshot = new File(artifactDir, "screencap_after.png");
        assertTrue("FTL screenshot capture failed", device.takeScreenshot(screenshot));
        assertTrue("FTL screenshot is empty", screenshot.isFile() && screenshot.length() > 0L);

        logcat = runShell(device, "logcat -d");
        writeText(new File(artifactDir, "logcat_after.txt"), logcat);

        requireMarker(logcat, "IMM Android renderer API: " + renderer);
        requireMarker(logcat, "IMMAVAL validation render size: " + VALIDATION_RENDER_WIDTH + "x" + VALIDATION_RENDER_HEIGHT);
        requireMarker(logcat, "IMMAVAL validation playback");
        requireMarker(logcat, "IMMAVAL loadPath result=1");
        requireMarker(logcat, "IMMAVAL native render capture written");
        requireMarker(logcat, "Loaded in CPU");
        requireMarker(logcat, "Loaded in GPU");

        if ("Vulkan".equals(renderer)) {
            requireMarker(logcat, "Vulkan renderer created Android surface");
            requireMarker(logcat, "Vulkan renderer initialized with owned device");
            requireMarker(logcat, "Vulkan renderer submitted picture draw commands");
            requireMarker(logcat, "Vulkan renderer submitted static paint draw commands");
        }

        forbidMarker(logcat, "Vulkan renderer failed");
        forbidMarker(logcat, "Vulkan renderer could not");
        forbidMarker(logcat, "Vulkan draw submission is not implemented yet");
        forbidMarker(logcat, "Could not initialize piRenderer");
        forbidMarker(logcat, "Failed to load IMM");
        forbidMarker(logcat, "Fatal signal");

        File sampleCapture = new File(artifactDir, "sample-native-render-after.ppm");
        Files.copy(nativeCapture.toPath(), sampleCapture.toPath(), StandardCopyOption.REPLACE_EXISTING);

        runShell(device, "am force-stop " + PACKAGE_NAME);
        runShell(device, "logcat -c");
        assertTrue("Could not remove stale native capture", !nativeCapture.exists() || nativeCapture.delete());

        File faceDocument = new File(targetContext.getFilesDir(), "face-orientation.imm");
        assertTrue("Face-orientation fixture was not extracted: " + faceDocument, faceDocument.isFile());
        Intent faceIntent = targetContext.getPackageManager().getLaunchIntentForPackage(PACKAGE_NAME);
        assertTrue("Face-orientation launch intent not found for " + PACKAGE_NAME, faceIntent != null);
        faceIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        faceIntent.putExtra("RenderingAPI", renderer);
        faceIntent.putExtra("ValidationRenderWidth", VALIDATION_RENDER_WIDTH);
        faceIntent.putExtra("ValidationRenderHeight", VALIDATION_RENDER_HEIGHT);
        faceIntent.putExtra("ValidationFixedDt", validationFixedDt);
        faceIntent.putExtra("ValidationPlayerFrame", validationPlayerFrame);
        faceIntent.putExtra("QUILL_PATH", faceDocument.getAbsolutePath());
        targetContext.startActivity(faceIntent);

        String faceLogcat = waitForNativeCapture(device, nativeCapture, waitSeconds);
        File faceCapture = new File(artifactDir, "face-orientation.ppm");
        Files.copy(nativeCapture.toPath(), faceCapture.toPath(), StandardCopyOption.REPLACE_EXISTING);
        writeText(new File(artifactDir, "face-orientation-logcat.txt"), faceLogcat);
        requireMarker(faceLogcat, "IMMAVAL loadPath result=1");
        requireMarker(faceLogcat, "IMMAVAL native render capture written");
        forbidMarker(faceLogcat, "Failed to load IMM");
        forbidMarker(faceLogcat, "Fatal signal");

        Files.copy(sampleCapture.toPath(), nativeCapture.toPath(), StandardCopyOption.REPLACE_EXISTING);
    }

    private static String runShell(UiDevice device, String command) throws Exception {
        return device.executeShellCommand(command);
    }

    private static void writeText(File path, String content) throws Exception {
        try (FileOutputStream stream = new FileOutputStream(path)) {
            stream.write(content.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void requireMarker(String logcat, String marker) {
        assertTrue("Missing required log marker: " + marker, logcat.contains(marker));
    }

    private static void forbidMarker(String logcat, String marker) {
        assertTrue("Forbidden log marker was present: " + marker, !logcat.contains(marker));
    }

    private static String waitForNativeCapture(UiDevice device, File capture, int waitSeconds) throws Exception {
        long deadline = SystemClock.elapsedRealtime() + waitSeconds * 1000L;
        String logcat = "";
        while (SystemClock.elapsedRealtime() < deadline) {
            logcat = runShell(device, "logcat -d");
            if (capture.isFile() && capture.length() > 0L && logcat.contains("IMMAVAL native render capture written")) {
                return logcat;
            }
            SystemClock.sleep(500L);
        }
        assertTrue("Native render capture was not written: " + capture + "\n" + logcat, false);
        return logcat;
    }

    private static int parseInt(String value, int fallback) {
        if (value == null || value.isEmpty()) {
            return fallback;
        }
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException ignored) {
            return fallback;
        }
    }

    private static long parseLong(String value, long fallback) {
        if (value == null || value.isEmpty()) {
            return fallback;
        }
        try {
            return Long.parseLong(value);
        } catch (NumberFormatException ignored) {
            return fallback;
        }
    }

    private static double parseDouble(String value, double fallback) {
        if (value == null || value.isEmpty()) {
            return fallback;
        }
        try {
            return Double.parseDouble(value);
        } catch (NumberFormatException ignored) {
            return fallback;
        }
    }
}
