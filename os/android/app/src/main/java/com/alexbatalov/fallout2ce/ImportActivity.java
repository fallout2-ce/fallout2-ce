package com.alexbatalov.fallout2ce;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.Toast;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;

public class ImportActivity extends Activity {
    private static final int IMPORT_REQUEST_CODE = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        launchImportPicker();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent resultData) {
        if (requestCode == IMPORT_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK) {
                final Uri treeUri = resultData != null ? resultData.getData() : null;
                if (treeUri != null) {
                    grantImportPermissions(treeUri, resultData);
                    final DocumentFile treeDocument = DocumentFile.fromTreeUri(this, treeUri);
                    if (treeDocument != null && treeDocument.isDirectory()) {
                        copyFiles(treeDocument);
                        return;
                    }
                }
            }

            finish();
        } else {
            super.onActivityResult(requestCode, resultCode, resultData);
        }
    }

    private void copyFiles(DocumentFile treeDocument) {
        final ProgressDialog dialog = createProgressDialog();
        dialog.show();

        new Thread(() -> {
            final ContentResolver contentResolver = getContentResolver();
            final File externalFilesDir = getExternalFilesDir(null);
            final boolean success = externalFilesDir != null
                && FileUtils.copyRecursively(contentResolver, treeDocument, externalFilesDir);

            runOnUiThread(() -> {
                dialog.dismiss();

                if (success) {
                    startMainActivity();
                } else {
                    Toast.makeText(this, R.string.import_failed, Toast.LENGTH_LONG).show();
                }

                finish();
            });
        }).start();
    }

    private void launchImportPicker() {
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
            | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
            | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);

        try {
            startActivityForResult(intent, IMPORT_REQUEST_CODE);
        } catch (ActivityNotFoundException e) {
            Toast.makeText(this, R.string.import_picker_unavailable, Toast.LENGTH_LONG).show();
            finish();
        }
    }

    private void grantImportPermissions(Uri treeUri, Intent resultData) {
        int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
        if (resultData != null) {
            flags = resultData.getFlags()
                & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        }

        try {
            getContentResolver().takePersistableUriPermission(treeUri, flags);
        } catch (SecurityException ignored) {
            // Some providers do not offer persistable grants. One-shot access is enough for import.
        }
    }

    private void startMainActivity() {
        Intent intent = new Intent(this, MainActivity.class);
        startActivity(intent);
    }

    private ProgressDialog createProgressDialog() {
        ProgressDialog progressDialog = new ProgressDialog(this,
            android.R.style.Theme_Material_Light_Dialog);
        progressDialog.setTitle(R.string.loading_dialog_title);
        progressDialog.setMessage(getString(R.string.loading_dialog_message));
        progressDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        progressDialog.setCancelable(false);

        return progressDialog;
    }
}
