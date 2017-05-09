/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.android.ble_fota;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ExpandableListView;
import android.widget.SimpleExpandableListAdapter;
import android.widget.TextView;

import com.mediatek.wearable.WearableListener;
import com.mediatek.wearable.WearableManager;
import com.mediatek.ctrl.fota.common.FotaOperator;
import com.mediatek.ctrl.fota.common.FotaVersion;
import com.mediatek.ctrl.fota.common.IFotaOperatorCallback;

/**
 * For a given BLE device, this Activity provides the user interface to connect, display data,
 * and display GATT services and characteristics supported by the device.  The Activity
 * communicates with {@code BluetoothLeService}, which in turn interacts with the
 * Bluetooth LE API.
 */
public class DeviceControlActivity extends Activity {
    private final static String TAG = DeviceControlActivity.class.getSimpleName();

    public static final String EXTRAS_DEVICE_NAME = "DEVICE_NAME";
    public static final String EXTRAS_DEVICE_ADDRESS = "DEVICE_ADDRESS";
    private static final int OPEN_BIN_CODE = 1337;

    private TextView mConnectionState;
    private TextView mDataField;
    private String mDeviceName;
    private String mDeviceAddress;
    private ExpandableListView mGattServicesList;
    private boolean mConnected = false;

    private final String LIST_NAME = "NAME";
    private final String LIST_UUID = "UUID";

    private Uri mSelectFileUri;
    public static boolean sIsSending = false;

    private static final int MGS_TEXT_VIEW_UPDATE   = 10;
    private static final int MSG_PROGRESS_UPDATE    = 20;

    public static final int FOTA_SEND_VIA_BT_SUCCESS = 2;

    private void clearUI() {
        mGattServicesList.setAdapter((SimpleExpandableListAdapter) null);
        mDataField.setText(R.string.no_data);
    }

    private WearableListener mWearableListener = new WearableListener() {

        @Override
        public void onDeviceChange(BluetoothDevice device) {
        }

        @Override
        public void onConnectChange(int oldState, int newState) {

            if (newState == WearableManager.STATE_CONNECTED) {
                mConnected = true;
                updateConnectionState(R.string.connected);
                invalidateOptionsMenu();
            }
            else if (newState == WearableManager.STATE_CONNECT_LOST) {
                mConnected = false;
                updateConnectionState(R.string.disconnected);
                invalidateOptionsMenu();
                clearUI();
            }
        }


        @Override
        public void onDeviceScan(BluetoothDevice device) {
        }

        @Override
        public void onModeSwitch(int newMode) {
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.gatt_services_characteristics);

        final Intent intent = getIntent();
        mDeviceName = intent.getStringExtra(EXTRAS_DEVICE_NAME);
        mDeviceAddress = intent.getStringExtra(EXTRAS_DEVICE_ADDRESS);

        // Sets up UI references.
        ((TextView) findViewById(R.id.device_address)).setText(mDeviceAddress);
        mGattServicesList = (ExpandableListView) findViewById(R.id.gatt_services_list);
        mGattServicesList.setOnChildClickListener(new ExpandableListView.OnChildClickListener() {
            @Override
            public boolean onChildClick(ExpandableListView parent, View v, int groupPosition,
                                        int childPosition, long id) {
                return false;
            }
        });
        mConnectionState = (TextView) findViewById(R.id.connection_state);
        mDataField = (TextView) findViewById(R.id.data_value);

        getActionBar().setTitle(mDeviceName);
        getActionBar().setDisplayHomeAsUpEnabled(true);

        boolean isSuccess = WearableManager.getInstance().init(true, getApplicationContext(), null, 0);
        Log.d(TAG, "WearableManager init " + isSuccess);

        // make sure in DOGP mode
        if (WearableManager.getInstance().getWorkingMode() != WearableManager.MODE_DOGP) {
            WearableManager.getInstance().switchMode();
        }
        WearableManager.getInstance().registerWearableListener(mWearableListener);
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(mDeviceAddress);
        WearableManager.getInstance().setRemoteDevice(device);
        WearableManager.getInstance().connect();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.gatt_services, menu);
        if (mConnected) {
            menu.findItem(R.id.menu_connect).setVisible(false);
            menu.findItem(R.id.menu_disconnect).setVisible(true);
        } else {
            menu.findItem(R.id.menu_connect).setVisible(true);
            menu.findItem(R.id.menu_disconnect).setVisible(false);
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch(item.getItemId()) {
            case R.id.menu_connect:
                WearableManager.getInstance().connect();
                return true;
            case R.id.menu_disconnect:
                if(!sIsSending) {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    startActivityForResult(intent, OPEN_BIN_CODE);
                }
                return true;
            case android.R.id.home:
                onBackPressed();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void updateConnectionState(final int resourceId) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mConnectionState.setText(resourceId);
            }
        });
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent resultData) {
        //Log.i(TAG, "Received an \"Activity Result\"");
        // The ACTION_OPEN_DOCUMENT intent was sent with the request code READ_REQUEST_CODE.
        // If the request code seen here doesn't match, it's the response to some other intent,
        // and the below code shouldn't run at all.

        if (requestCode == OPEN_BIN_CODE && resultCode == Activity.RESULT_OK) {
            // The document selected by the user won't be returned in the intent.
            // Instead, a URI to that document will be contained in the return intent
            // provided to this method as a parameter.  Pull that uri using "resultData.getData()"
            Uri uri = null;
            if (resultData != null) {
                uri = resultData.getData();
                Log.i(TAG, "Selected file path: " + uri.getPath());
                mSelectFileUri = uri;
                FotaOperator.getInstance(this).registerFotaCallback(mFotaCallback);
                mTransferTask.execute();
            }
        }
    }

    private Handler mHandler = new Handler() {

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            Log.d(TAG, "[handleMessage] msg.what" + msg.what);
            switch (msg.what) {
                case MGS_TEXT_VIEW_UPDATE:
                    String disp;
                    switch(msg.arg1) {
                        case FOTA_SEND_VIA_BT_SUCCESS:
                            disp = "Send success";
                            break;
                        default:
                            disp = "Error";
                            break;
                    }
                    mDataField.setText(disp);
                    break;

                case MSG_PROGRESS_UPDATE:
                    mDataField.setText(String.format("Updating:%d%%", msg.arg1));
                    break;

                default:
                    return;
            }
        }
    };

    private AsyncTask<Void, Void, Void> mTransferTask = new AsyncTask<Void, Void, Void>() {

        @Override
        protected Void doInBackground(Void... parameters) {
            sIsSending = true;
            Log.d(TAG, "[doInBackground] begin Fota Transferring");
            FotaOperator.getInstance(DeviceControlActivity.this).sendFotaFirmwareData(FotaOperator.TYPE_FIRMWARE_FULL_BIN, mSelectFileUri);
            return null;
        }
    };

    private IFotaOperatorCallback mFotaCallback = new IFotaOperatorCallback() {

        @Override
        public void onCustomerInfoReceived(String information) {
        }

        @Override
        public void onFotaVersionReceived(FotaVersion version) {
        }

        @Override
        public void onStatusReceived(int status) {
            Log.d(TAG, "[onStatusReceived] status : " + status);

            Message msg = mHandler.obtainMessage(MGS_TEXT_VIEW_UPDATE, status, 0);
            mHandler.sendMessage(msg);
        }

        @Override
        public void onProgress(int progress) {
            Log.d(TAG, "[onProgress] progress : " + progress);

            Message msg = mHandler.obtainMessage(MSG_PROGRESS_UPDATE, progress, 0);
            mHandler.sendMessage(msg);
        }

        @Override
        public void onConnectionStateChange(int newConnectionState) {
        }
    };
}
