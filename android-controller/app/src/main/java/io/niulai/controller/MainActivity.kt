package io.niulai.controller

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.runtime.getValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.niulai.controller.ui.ControllerScreen
import io.niulai.controller.ui.ControllerViewModel

class MainActivity : ComponentActivity() {
    private val viewModel: ControllerViewModel by viewModels()

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { grants ->
        if (requiredPermissions().all { grants[it] == true || hasPermission(it) }) {
            viewModel.startScan()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.i(NiulaiApplication.LOG_TAG, "ACTIVITY_ON_CREATE restored=${savedInstanceState != null}")
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setContent {
            val state by viewModel.uiState.collectAsStateWithLifecycle()
            ControllerScreen(
                state = state,
                onScan = ::scanWithPermissions,
                onStopScan = viewModel::stopScan,
                onConnect = viewModel::connect,
                onDisconnect = viewModel::disconnect,
                onHome = viewModel::requestHome,
                onPointerDown = viewModel::pointerDown,
                onPointerUp = viewModel::pointerUp,
                onCancelLeg = viewModel::cancelLeg,
                onClearControls = viewModel::clearControls,
                onStartTestGameplay = viewModel::startTestGameplay,
                onStartPhysicalGameplay = viewModel::startPhysicalGameplay,
                onEndTestGameplay = viewModel::endTestGameplay,
                onPlayMama = viewModel::playMama,
                onRematchGameplay = viewModel::rematchGameplay,
                onToggleGameplayMode = viewModel::toggleGameplayMode,
                onSetGameplayMuted = viewModel::setGameplayMuted,
                onSaveFeedbackSettings = viewModel::saveFeedbackSettings,
                onLeaveTestGameplay = viewModel::leaveTestGameplay,
            )
        }
    }

    override fun onStart() {
        super.onStart()
        Log.i(NiulaiApplication.LOG_TAG, "ACTIVITY_ON_START")
        viewModel.setControllerActive(true)
    }

    override fun onStop() {
        Log.i(NiulaiApplication.LOG_TAG, "ACTIVITY_ON_STOP")
        viewModel.setControllerActive(false)
        super.onStop()
    }

    private fun scanWithPermissions() {
        val missing = requiredPermissions().filterNot(::hasPermission)
        Log.i(NiulaiApplication.LOG_TAG, "SCAN_REQUEST missingPermissions=${missing.size}")
        if (missing.isEmpty()) {
            viewModel.startScan()
        } else {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun requiredPermissions(): List<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            listOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    private fun hasPermission(permission: String): Boolean =
        ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED
}
