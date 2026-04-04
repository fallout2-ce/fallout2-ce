package com.alexbatalov.fallout2ce;

import android.content.ContentResolver;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class FileUtils {

    static boolean copyRecursively(ContentResolver contentResolver, DocumentFile src, File dest) {
        if (dest == null || (!dest.exists() && !dest.mkdirs())) {
            return false;
        }

        final DocumentFile[] documentFiles = src.listFiles();
        for (final DocumentFile documentFile : documentFiles) {
            final String name = documentFile.getName();
            if (name == null || name.isEmpty()) {
                return false;
            }

            if (documentFile.isFile()) {
                if (!copyFile(contentResolver, documentFile, new File(dest, name))) {
                    return false;
                }
            } else if (documentFile.isDirectory()) {
                final File subdirectory = new File(dest, name);
                if (!copyRecursively(contentResolver, documentFile, subdirectory)) {
                    return false;
                }
            }
        }
        return true;
    }

    private static boolean copyFile(ContentResolver contentResolver, DocumentFile src, File dest) {
        final File parent = dest.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            return false;
        }

        try (final InputStream inputStream = contentResolver.openInputStream(src.getUri());
             final OutputStream outputStream = new FileOutputStream(dest)) {
            if (inputStream == null) {
                return false;
            }

            final byte[] buffer = new byte[16384];
            int bytesRead;
            while ((bytesRead = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, bytesRead);
            }
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }

        return true;
    }
}
