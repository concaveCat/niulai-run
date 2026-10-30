package io.niulai.controller.ui

import androidx.activity.compose.BackHandler
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.ui.window.Dialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextButton
import androidx.compose.material3.Typography
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.niulai.controller.R
import io.niulai.controller.ble.BlePhase
import io.niulai.controller.ble.NiulaiDevice
import io.niulai.controller.feedback.GameplayFeedbackSettings
import io.niulai.controller.gameplay.AiFeedbackStatus
import io.niulai.controller.gameplay.GameplayMode
import io.niulai.controller.gameplay.GameplayPhase
import io.niulai.controller.gameplay.GameplayUiState
import io.niulai.controller.protocol.Leg
import io.niulai.controller.protocol.StatusPacket
import io.niulai.controller.protocol.AccelerationTelemetry
import kotlin.math.cos
import kotlin.math.sin

private val NiulaiFontFamily = FontFamily(Font(R.font.niulai_font))
private val DefaultTypography = Typography()
private val NiulaiTypography = Typography(
    displayLarge = DefaultTypography.displayLarge.withNiulaiFont(),
    displayMedium = DefaultTypography.displayMedium.withNiulaiFont(),
    displaySmall = DefaultTypography.displaySmall.withNiulaiFont(),
    headlineLarge = DefaultTypography.headlineLarge.withNiulaiFont(),
    headlineMedium = DefaultTypography.headlineMedium.withNiulaiFont(),
    headlineSmall = DefaultTypography.headlineSmall.withNiulaiFont(),
    titleLarge = DefaultTypography.titleLarge.withNiulaiFont(),
    titleMedium = DefaultTypography.titleMedium.withNiulaiFont(),
    titleSmall = DefaultTypography.titleSmall.withNiulaiFont(),
    bodyLarge = DefaultTypography.bodyLarge.withNiulaiFont(),
    bodyMedium = DefaultTypography.bodyMedium.withNiulaiFont(),
    bodySmall = DefaultTypography.bodySmall.withNiulaiFont(),
    labelLarge = DefaultTypography.labelLarge.withNiulaiFont(),
    labelMedium = DefaultTypography.labelMedium.withNiulaiFont(),
    labelSmall = DefaultTypography.labelSmall.withNiulaiFont(),
)

private val NiulaiColorScheme = lightColorScheme(
    primary = Color(0xff7a4b22),
    onPrimary = Color.White,
    secondary = Color(0xff315d2a),
    onSecondary = Color.White,
    surface = Color(0xfffff8df),
    onSurface = Color(0xff25170d),
)

private fun TextStyle.withNiulaiFont(): TextStyle = copy(fontFamily = NiulaiFontFamily)

private const val MAIN_COVER_ASPECT = 1080f / 1920f
private const val CTRL_BACKGROUND_ASPECT = 1080f / 1920f
private val LANDSCAPE_LEG_BUTTON_HEIGHT = 72.dp
internal const val GAMEPLAY_END_BUTTON_LABEL = "结束游戏"
internal const val GAMEPLAY_MAMA_BUTTON_LABEL = "牛来"
internal const val GAMEPLAY_HOME_BUTTON_LABEL = "归位"

internal data class CropGeometry(
    val renderedWidth: Dp,
    val renderedHeight: Dp,
    val offsetX: Dp,
    val offsetY: Dp,
)

/** ContentScale.Crop geometry, expressed in the same coordinates as overlays. */
internal fun cropGeometry(
    viewportWidth: Dp,
    viewportHeight: Dp,
    imageAspect: Float,
): CropGeometry {
    val viewportAspect = viewportWidth / viewportHeight
    return if (viewportAspect > imageAspect) {
        val renderedWidth = viewportWidth
        val renderedHeight = viewportWidth / imageAspect
        CropGeometry(
            renderedWidth = renderedWidth,
            renderedHeight = renderedHeight,
            offsetX = 0.dp,
            offsetY = (viewportHeight - renderedHeight) / 2,
        )
    } else {
        val renderedHeight = viewportHeight
        val renderedWidth = viewportHeight * imageAspect
        CropGeometry(
            renderedWidth = renderedWidth,
            renderedHeight = renderedHeight,
            offsetX = (viewportWidth - renderedWidth) / 2,
            offsetY = 0.dp,
        )
    }
}

internal enum class AppScreen {
    MAIN,
    CONNECT,
    CTRL,
    TEST,
    SETTINGS,
}

internal data class ControlLayout(
    val frontLeftX: Float,
    val frontRightX: Float,
    val rearLeftX: Float,
    val rearRightX: Float,
    val frontY: Float,
    val rearY: Float,
)

internal fun controlLayout(isLandscape: Boolean): ControlLayout = if (isLandscape) {
    // Physical screen positions follow the semantic labels: left legs stay on
    // the left and right legs stay on the right. Keep semantic BLE bits unchanged.
    ControlLayout(
        frontLeftX = 0.30f,
        frontRightX = 0.70f,
        rearLeftX = 0.30f,
        rearRightX = 0.70f,
        frontY = 0.43f,
        rearY = 0.57f,
    )
} else {
    ControlLayout(
        frontLeftX = 0.33f,
        frontRightX = 0.67f,
        rearLeftX = 0.25f,
        rearRightX = 0.75f,
        frontY = 0.46f,
        rearY = 0.70f,
    )
}

internal fun screenAfterBlePhase(current: AppScreen, phase: BlePhase): AppScreen = when {
    phase == BlePhase.CONNECTED -> AppScreen.CTRL
    current == AppScreen.CTRL -> AppScreen.CONNECT
    else -> current
}

internal fun contactStatusText(contactMask: Int): String {
    val labels = Leg.values()
        .filter { contactMask and it.bit != 0 }
        .joinToString("/") { it.shortLabel }
    return if (labels.isEmpty()) {
        "已连接 · 等待 KW10 落脚"
    } else {
        "已连接 · 完美触碰：$labels"
    }
}

internal fun contactRisingMask(previousMask: Int, currentMask: Int): Int =
    currentMask and previousMask.inv() and 0x0f

internal fun controllerStatusText(status: StatusPacket?): String = when {
    status == null -> "已连接 · 等待 STATUS"
    status.fallDetected -> "摔倒保护 · 本局失败"
    status.failsafeActive -> "安全停止 · FAILSAFE"
    else -> contactStatusText(status.contactMask)
}

internal fun statusDiagnosticText(status: StatusPacket?): String = status?.let {
    "STATUS seq=${it.sequence} · applied=0x${it.appliedLegMask.toString(16).uppercase().padStart(2, '0')}"
} ?: "STATUS 等待中"

internal fun bleControlsEnabled(
    phase: BlePhase,
    failsafeActive: Boolean,
    fallDetected: Boolean = false,
): Boolean = phase == BlePhase.CONNECTED && !failsafeActive && !fallDetected

internal fun contactMaskForControl(phase: BlePhase, status: StatusPacket?): Int =
    if (phase == BlePhase.CONNECTED) status?.contactMask ?: 0 else 0

internal fun accelerationText(telemetry: AccelerationTelemetry?): String = when {
    telemetry == null -> "动态加速度：等待 MPU6500"
    !telemetry.valid -> "动态加速度：校准中"
    telemetry.stale -> "动态加速度：数据过期"
    else -> "动态加速度：%.2f g".format(telemetry.dynamicAccelerationG)
}

internal fun accelerationColor(telemetry: AccelerationTelemetry?): Color = when {
    telemetry == null || !telemetry.valid || telemetry.stale -> Color(0xffb7b7b7)
    telemetry.clipped || telemetry.dynamicAccelerationG > 0.60f -> Color(0xffe33b32)
    telemetry.dynamicAccelerationG > 0.25f -> Color(0xffffc928)
    else -> Color(0xff82d66f)
}

internal fun gameplayResultModeText(mode: GameplayMode, isSimulated: Boolean): String {
    val modeText = if (mode == GameplayMode.SPLIT) "双人模式" else "单人模式"
    val sourceText = if (isSimulated) "模拟数据" else "实体输入记录"
    return "$modeText · $sourceText"
}

internal fun gameplayModeButtonLabel(mode: GameplayMode): String =
    if (mode == GameplayMode.SOLO) "单人模式" else "双人模式"

internal fun gameplayModeButtonColor(mode: GameplayMode): Color =
    if (mode == GameplayMode.SOLO) {
        Color(0xffeef8dc)
    } else {
        Color(0xffffe7bf)
    }

internal fun gameplayStepsText(completedSteps: Int): String =
    "迈出了${completedSteps.coerceAtLeast(0)}步"

internal fun gameplaySessionScreen(isSimulated: Boolean): AppScreen =
    if (isSimulated) AppScreen.TEST else AppScreen.CTRL

internal fun gameplayResultExitScreen(): AppScreen = AppScreen.MAIN

@Composable
fun ControllerScreen(
    state: ControllerUiState,
    onScan: () -> Unit,
    onStopScan: () -> Unit,
    onConnect: (String) -> Unit,
    onDisconnect: () -> Unit,
    onHome: () -> Unit,
    onPointerDown: (Long, Leg) -> Unit,
    onPointerUp: (Long) -> Unit,
    onCancelLeg: (Leg) -> Unit,
    onClearControls: () -> Unit,
    onStartTestGameplay: () -> Unit,
    onStartPhysicalGameplay: () -> Unit,
    onEndTestGameplay: () -> Unit,
    onPlayMama: () -> Unit,
    onRematchGameplay: () -> Unit,
    onToggleGameplayMode: () -> Unit,
    onSetGameplayMuted: (Boolean) -> Unit,
    onSaveFeedbackSettings: (String, String, String, Boolean) -> Unit,
    onLeaveTestGameplay: () -> Unit,
) {
    var screenName by rememberSaveable { mutableStateOf(AppScreen.MAIN.name) }
    val screen = AppScreen.valueOf(screenName)

    LaunchedEffect(state.ble.phase, state.gameplay.phase) {
        /* Physical game end intentionally disconnects only after the HOME
         * settle window. Keep the result page visible while that safety stop
         * changes the BLE phase to DISCONNECTED. */
        if (state.gameplay.phase != GameplayPhase.RESULT) {
            screenName = screenAfterBlePhase(screen, state.ble.phase).name
        }
    }

    BackHandler(enabled = screen != AppScreen.MAIN) {
        when (screen) {
            AppScreen.CTRL -> {
                onDisconnect()
                screenName = AppScreen.CONNECT.name
            }
            AppScreen.TEST -> {
                val returnScreen = gameplaySessionScreen(state.gameplay.isSimulated)
                onLeaveTestGameplay()
                onClearControls()
                screenName = if (returnScreen == AppScreen.CTRL) {
                    returnScreen.name
                } else {
                    AppScreen.MAIN.name
                }
            }
            AppScreen.CONNECT -> {
                onStopScan()
                screenName = AppScreen.MAIN.name
            }
            AppScreen.SETTINGS -> screenName = AppScreen.MAIN.name
            AppScreen.MAIN -> Unit
        }
    }

    NiulaiTheme {
        when (screen) {
            AppScreen.MAIN -> MainPage(
                onStart = { screenName = AppScreen.CONNECT.name },
                onTest = {
                    onClearControls()
                    screenName = AppScreen.TEST.name
                },
                onSettings = { screenName = AppScreen.SETTINGS.name },
            )

            AppScreen.SETTINGS -> SettingsPage(
                settings = state.feedbackSettings,
                onBack = { screenName = AppScreen.MAIN.name },
                onSave = onSaveFeedbackSettings,
            )

            AppScreen.CONNECT -> ConnectPage(
                state = state,
                onBack = {
                    onStopScan()
                    screenName = AppScreen.MAIN.name
                },
                onScan = onScan,
                onStopScan = onStopScan,
                onConnect = onConnect,
            )

            AppScreen.CTRL -> when (state.gameplay.phase) {
                GameplayPhase.RESULT -> ResultPage(
                    state = state.gameplay,
                    onRematch = {
                        onRematchGameplay()
                        screenName = AppScreen.CTRL.name
                    },
                    onMute = onSetGameplayMuted,
                    onExit = {
                        onLeaveTestGameplay()
                        onClearControls()
                        screenName = gameplayResultExitScreen().name
                    },
                )

                GameplayPhase.IDLE,
                GameplayPhase.PLAYING,
                -> ControlPage(
                    state = state,
                    testMode = false,
                    onHome = onHome,
                    onPlayMama = onPlayMama,
                    onToggleGameplayMode = onToggleGameplayMode,
                    onTutorialComplete = onStartPhysicalGameplay,
                    onEndGameplay = onEndTestGameplay,
                    onPointerDown = onPointerDown,
                    onPointerUp = onPointerUp,
                    onCancelLeg = onCancelLeg,
                )
            }

            AppScreen.TEST -> when (state.gameplay.phase) {
                GameplayPhase.RESULT -> ResultPage(
                    state = state.gameplay,
                    onRematch = {
                        val returnScreen = gameplaySessionScreen(state.gameplay.isSimulated)
                        onRematchGameplay()
                        screenName = returnScreen.name
                    },
                    onMute = onSetGameplayMuted,
                    onExit = {
                        onLeaveTestGameplay()
                        onClearControls()
                        screenName = gameplayResultExitScreen().name
                    },
                )

                GameplayPhase.IDLE,
                GameplayPhase.PLAYING,
                -> ControlPage(
                    state = state,
                    testMode = true,
                    onHome = onClearControls,
                    onPlayMama = onPlayMama,
                    onTutorialComplete = onStartTestGameplay,
                    onEndGameplay = onEndTestGameplay,
                    onToggleGameplayMode = onToggleGameplayMode,
                    onPointerDown = onPointerDown,
                    onPointerUp = onPointerUp,
                    onCancelLeg = onCancelLeg,
                )
            }
        }
    }
}

@Composable
private fun NiulaiTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = NiulaiColorScheme,
        typography = NiulaiTypography,
        content = content,
    )
}

@Composable
private fun BackgroundImage(resourceId: Int) {
    Image(
        painter = painterResource(resourceId),
        contentDescription = null,
        contentScale = ContentScale.Crop,
        alignment = Alignment.Center,
        modifier = Modifier.fillMaxSize(),
    )
}

@Composable
private fun MainPage(
    onStart: () -> Unit,
    onTest: () -> Unit,
    onSettings: () -> Unit,
) {
    BoxWithConstraints(modifier = Modifier.fillMaxSize().clip(RoundedCornerShape(0.dp))) {
        val reference = cropGeometry(maxWidth, maxHeight, MAIN_COVER_ASPECT)
        val landscape = maxWidth > maxHeight
        val startModifier = if (landscape) {
            Modifier
                .align(Alignment.BottomStart)
                .padding(start = maxWidth * 0.12f, bottom = maxHeight * 0.06f)
                .width(maxWidth * 0.30f)
                .height(maxHeight * 0.19f)
        } else {
            Modifier
                .width(reference.renderedWidth * (553f / 1080f))
                .height(reference.renderedHeight * (495f / 1920f))
                .offset(
                    x = reference.offsetX + reference.renderedWidth * (272f / 1080f),
                    y = reference.offsetY + reference.renderedHeight * (1115f / 1920f),
                )
        }
        val testModifier = if (landscape) {
            Modifier
                .align(Alignment.BottomEnd)
                .padding(end = maxWidth * 0.12f, bottom = maxHeight * 0.06f)
                .width(maxWidth * 0.40f)
                .height(maxHeight * 0.19f)
        } else {
            Modifier
                .width(reference.renderedWidth * (650f / 1080f))
                .height(reference.renderedHeight * (128f / 1920f))
                .offset(
                    x = reference.offsetX + reference.renderedWidth * (215f / 1080f),
                    y = reference.offsetY + reference.renderedHeight * (1695f / 1920f),
                )
        }

        // The supplied cover already contains the complete title artwork.
        // ContentScale.Crop keeps it proportional and crops only the overflow.
        BackgroundImage(R.drawable.main_cover)

        TextButton(
            onClick = onSettings,
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(
                    top = WindowInsets.systemBars.asPaddingValues().calculateTopPadding() + 8.dp,
                    end = 16.dp,
                )
                .heightIn(min = 52.dp),
            colors = ButtonDefaults.textButtonColors(
                containerColor = Color(0xccfff6c8),
                contentColor = Color(0xff402714),
            ),
        ) {
            Text("设置", fontSize = 20.sp)
        }

        Button(
            onClick = onStart,
            shape = RoundedCornerShape(34.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = Color(0xd9fff6c8),
                contentColor = Color(0xff171006),
            ),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(0.dp),
            modifier = startModifier,
        ) {
            Text(
                text = "开始",
                fontSize = 52.sp,
                color = Color(0xff171006),
            )
        }

        Button(
            onClick = onTest,
            shape = RoundedCornerShape(22.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = Color(0xd92c5a36),
                contentColor = Color.White,
            ),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(0.dp),
            modifier = testModifier,
        ) {
            Text(
                text = "不连接设备，进入测试",
                fontSize = 25.sp,
            )
        }
    }
}

@Composable
private fun SettingsPage(
    settings: GameplayFeedbackSettings,
    onBack: () -> Unit,
    onSave: (String, String, String, Boolean) -> Unit,
) {
    var apiUrl by rememberSaveable(settings.apiUrl) { mutableStateOf(settings.apiUrl) }
    var apiKey by rememberSaveable(settings.apiKey) { mutableStateOf(settings.apiKey) }
    var model by rememberSaveable(settings.model) { mutableStateOf(settings.model) }
    var useProxy by rememberSaveable(settings.useProxy) { mutableStateOf(settings.useProxy) }
    var showApiKey by rememberSaveable { mutableStateOf(false) }
    var saved by rememberSaveable { mutableStateOf(false) }

    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        val landscape = maxWidth > maxHeight
        BackgroundImage(R.drawable.main_cover)
        Surface(
            modifier = Modifier
                .align(Alignment.Center)
                .fillMaxWidth(if (landscape) 0.82f else 0.92f)
                .heightIn(min = 390.dp, max = maxHeight * 0.92f),
            shape = RoundedCornerShape(26.dp),
            color = Color(0xebfff8df),
            shadowElevation = 12.dp,
        ) {
            LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 20.dp, vertical = 16.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                item {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween,
                    ) {
                        Text("评价服务设置", color = Color(0xff402714), fontSize = 28.sp)
                        TextButton(
                            onClick = onBack,
                            modifier = Modifier.heightIn(min = 52.dp),
                        ) { Text("返回", color = Color(0xff765335), fontSize = 17.sp) }
                    }
                }
                item {
                    Text(
                        "默认已填入 DeepSeek：填写 API Key 后直连 Chat Completions。若使用本地代理，请打开代理模式并留空 API Key。",
                        color = Color(0xff765335),
                        fontSize = 14.sp,
                        modifier = Modifier.heightIn(min = 52.dp),
                    )
                }
                item {
                    TextField(
                        value = apiUrl,
                        onValueChange = {
                            apiUrl = it
                            saved = false
                        },
                        modifier = Modifier.fillMaxWidth().heightIn(min = 68.dp),
                        singleLine = true,
                        label = { Text("API URL") },
                        placeholder = { Text("https://api.deepseek.com/v1") },
                    )
                }
                item {
                    TextField(
                        value = apiKey,
                        onValueChange = {
                            apiKey = it
                            saved = false
                        },
                        modifier = Modifier.fillMaxWidth().heightIn(min = 68.dp),
                        singleLine = true,
                        label = { Text("API Key") },
                        placeholder = { Text(if (useProxy) "代理模式下可留空" else "填入 DeepSeek API Key") },
                        visualTransformation = if (showApiKey) {
                            VisualTransformation.None
                        } else {
                            PasswordVisualTransformation()
                        },
                        trailingIcon = {
                            TextButton(onClick = { showApiKey = !showApiKey }) {
                                Text(if (showApiKey) "隐藏" else "显示")
                            }
                        },
                    )
                }
                item {
                    TextField(
                        value = model,
                        onValueChange = {
                            model = it
                            saved = false
                        },
                        modifier = Modifier.fillMaxWidth().heightIn(min = 68.dp),
                        singleLine = true,
                        label = { Text("模型名称") },
                        placeholder = { Text("deepseek-chat") },
                    )
                }
                item {
                    Row(
                        modifier = Modifier.fillMaxWidth().heightIn(min = 68.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text("使用本地代理", color = Color(0xff402714), fontSize = 16.sp)
                            Text(
                                "关闭时按 DeepSeek / OpenAI 兼容接口直连",
                                color = Color(0xff765335),
                                fontSize = 12.sp,
                            )
                        }
                        Switch(
                            checked = useProxy,
                            onCheckedChange = {
                                useProxy = it
                                saved = false
                            },
                        )
                    }
                }
                item {
                    Text(
                        "API Key 使用 Android Keystore 加密保存，不会显示在日志中。建议优先使用权限受限、可轮换的测试 Key。",
                        color = Color(0xff627751),
                        fontSize = 13.sp,
                        modifier = Modifier.heightIn(min = 40.dp),
                    )
                }
                item {
                    Button(
                        onClick = {
                            onSave(apiUrl, apiKey, model, useProxy)
                            saved = true
                        },
                        enabled = apiUrl.isNotBlank() && model.isNotBlank(),
                        modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xff315d2a),
                            contentColor = Color.White,
                        ),
                    ) { Text("保存设置", fontSize = 18.sp) }
                }
                if (saved) {
                    item {
                        Text(
                            "设置已保存。下一局结算时将使用新的评价服务。",
                            color = Color(0xff315d2a),
                            fontSize = 14.sp,
                            modifier = Modifier.heightIn(min = 32.dp),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ConnectPage(
    state: ControllerUiState,
    onBack: () -> Unit,
    onScan: () -> Unit,
    onStopScan: () -> Unit,
    onConnect: (String) -> Unit,
) {
    BoxWithConstraints(modifier = Modifier.fillMaxSize().clip(RoundedCornerShape(0.dp))) {
        BackgroundImage(R.drawable.connect)

        TextButton(
            onClick = onBack,
            modifier = Modifier
                .align(Alignment.TopStart)
                .padding(start = maxWidth * 0.05f, top = maxHeight * 0.025f),
            colors = ButtonDefaults.textButtonColors(
                contentColor = Color.White,
            ),
        ) {
            Text(text = "返回", fontSize = 22.sp)
        }

        Text(
            text = "连接设备",
            color = Color.White,
            fontSize = 42.sp,
            modifier = Modifier
                .align(Alignment.TopCenter)
                .offset(y = maxHeight * 0.085f),
        )

        val scanning = state.ble.phase == BlePhase.SCANNING
        Button(
            onClick = if (scanning) onStopScan else onScan,
            enabled = state.ble.phase != BlePhase.UNSUPPORTED &&
                state.ble.phase != BlePhase.CONNECTING,
            shape = RoundedCornerShape(30.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = Color(0xe6fff2b8),
                contentColor = Color(0xff21160b),
                disabledContainerColor = Color(0x99d8d0ad),
                disabledContentColor = Color(0xff6a675b),
            ),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(
                horizontal = 24.dp,
                vertical = 0.dp,
            ),
            modifier = Modifier
                .align(Alignment.TopCenter)
                .offset(y = maxHeight * 0.18f)
                .width(maxWidth * 0.72f)
                .height(maxHeight * 0.105f),
        ) {
            BluetoothIcon(modifier = Modifier.size(34.dp), color = Color(0xff21160b))
            Spacer(modifier = Modifier.width(12.dp))
            Text(
                text = if (scanning) "停止搜索" else "搜索蓝牙设备",
                fontSize = 29.sp,
            )
        }

        DevicePanel(
            state = state,
            onConnect = onConnect,
            modifier = Modifier
                .align(Alignment.TopCenter)
                .offset(y = maxHeight * 0.325f)
                .width(maxWidth * 0.86f)
                .height(maxHeight * 0.40f),
        )
    }
}

@Composable
private fun BluetoothIcon(modifier: Modifier, color: Color) {
    Canvas(modifier = modifier) {
        val centerX = size.width / 2f
        val centerY = size.height / 2f
        val strokeWidth = size.minDimension * 0.11f
        val path = Path().apply {
            moveTo(centerX, 0f)
            lineTo(centerX, size.height)
            moveTo(centerX, 0f)
            lineTo(size.width * 0.82f, centerY - size.height * 0.25f)
            lineTo(centerX, centerY)
            lineTo(size.width * 0.82f, centerY + size.height * 0.25f)
            lineTo(centerX, size.height)
            moveTo(centerX, centerY)
            lineTo(size.width * 0.18f, centerY - size.height * 0.25f)
            moveTo(centerX, centerY)
            lineTo(size.width * 0.18f, centerY + size.height * 0.25f)
        }
        drawPath(
            path = path,
            color = color,
            style = androidx.compose.ui.graphics.drawscope.Stroke(
                width = strokeWidth,
                cap = androidx.compose.ui.graphics.StrokeCap.Round,
                join = androidx.compose.ui.graphics.StrokeJoin.Round,
            ),
        )
    }
}

@Composable
private fun DevicePanel(
    state: ControllerUiState,
    onConnect: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(28.dp),
        color = Color(0xe6fff8d9),
        shadowElevation = 8.dp,
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 22.dp, vertical = 18.dp),
        ) {
            Text(
                text = "可连接的设备",
                color = Color(0xff402714),
                fontSize = 28.sp,
            )
            Spacer(modifier = Modifier.height(8.dp))

            if (state.ble.devices.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                    contentAlignment = Alignment.Center,
                ) {
                    when (state.ble.phase) {
                        BlePhase.SCANNING -> Column(
                            horizontalAlignment = Alignment.CenterHorizontally,
                        ) {
                            CircularProgressIndicator(
                                color = Color(0xff7a4b22),
                                strokeWidth = 4.dp,
                            )
                            Spacer(modifier = Modifier.height(10.dp))
                            Text("正在搜索…", color = Color(0xff624327), fontSize = 22.sp)
                        }

                        BlePhase.CONNECTING -> Text(
                            text = "正在连接设备…",
                            color = Color(0xff624327),
                            fontSize = 22.sp,
                        )

                        BlePhase.UNSUPPORTED -> Text(
                            text = "此设备不支持蓝牙",
                            color = Color(0xff8b3028),
                            fontSize = 22.sp,
                        )

                        else -> Text(
                            text = "点击上方按钮搜索设备",
                            color = Color(0xff624327),
                            fontSize = 22.sp,
                        )
                    }
                }
            } else {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    items(state.ble.devices, key = { it.address }) { device ->
                        DeviceRow(device = device, onConnect = onConnect)
                    }
                }
            }

            val message = when {
                state.ble.error != null -> state.ble.error
                state.ble.phase == BlePhase.CONNECTING -> "请稍候…"
                else -> null
            }
            if (message != null) {
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = message,
                    color = if (state.ble.error == null) Color(0xff624327) else Color(0xff8b3028),
                    fontSize = 18.sp,
                )
            }
        }
    }
}

@Composable
private fun DeviceRow(device: NiulaiDevice, onConnect: (String) -> Unit) {
    Button(
        onClick = { onConnect(device.address) },
        shape = RoundedCornerShape(18.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = Color(0xd97a4b22),
            contentColor = Color.White,
        ),
        contentPadding = androidx.compose.foundation.layout.PaddingValues(
            horizontal = 18.dp,
            vertical = 8.dp,
        ),
        modifier = Modifier
            .fillMaxWidth()
            .height(68.dp),
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            Text(text = device.name, fontSize = 22.sp)
            Text(text = device.address, fontSize = 14.sp)
        }
    }
}

@Composable
private fun ControlPage(
    state: ControllerUiState,
    testMode: Boolean,
    onHome: () -> Unit = {},
    onPlayMama: () -> Unit = {},
    onTutorialComplete: () -> Unit = {},
    onEndGameplay: () -> Unit = {},
    onToggleGameplayMode: () -> Unit = {},
    onPointerDown: (Long, Leg) -> Unit,
    onPointerUp: (Long) -> Unit,
    onCancelLeg: (Leg) -> Unit,
) {
    var showTutorial by rememberSaveable(testMode) { mutableStateOf(true) }
    var previousContactMask by rememberSaveable(testMode) { mutableStateOf(0) }
    var touchdownMask by rememberSaveable(testMode) { mutableStateOf(0) }
    var touchdownEffectNonce by rememberSaveable(testMode) { mutableStateOf(0) }

    val contactMask = contactMaskForControl(state.ble.phase, state.ble.lastStatus)
    val statusPacket = if (state.ble.phase == BlePhase.CONNECTED) {
        state.ble.lastStatus
    } else {
        null
    }
    val failsafeActive = statusPacket?.failsafeActive == true
    val fallDetected = statusPacket?.fallDetected == true
    val controlsEnabled = if (testMode) {
        state.gameplay.phase == GameplayPhase.PLAYING
    } else {
        bleControlsEnabled(state.ble.phase, failsafeActive, fallDetected) &&
            state.homeRequestStatus != HomeRequestStatus.SENDING
    }
    val homeEnabled = !testMode && state.ble.phase == BlePhase.CONNECTED &&
        state.homeRequestStatus != HomeRequestStatus.SENDING

    LaunchedEffect(testMode, state.ble.phase, contactMask) {
        val risingMask = contactRisingMask(previousContactMask, contactMask)
        previousContactMask = contactMask
        if (risingMask != 0) {
            touchdownMask = risingMask
            touchdownEffectNonce++
        }
    }

    LaunchedEffect(state.gameplay.phase) {
        if (state.gameplay.phase == GameplayPhase.PLAYING) {
            showTutorial = false
        }
    }

    LaunchedEffect(testMode, state.ble.phase, showTutorial) {
        if (!testMode &&
            state.ble.phase == BlePhase.CONNECTED &&
            !showTutorial &&
            state.gameplay.phase != GameplayPhase.PLAYING
        ) {
            onTutorialComplete()
        }
    }

    BoxWithConstraints(modifier = Modifier.fillMaxSize().clip(RoundedCornerShape(0.dp))) {
        val background = cropGeometry(maxWidth, maxHeight, CTRL_BACKGROUND_ASPECT)
        val landscape = maxWidth > maxHeight
        val layout = controlLayout(landscape)
        val statusBarInset = WindowInsets.systemBars.asPaddingValues().calculateTopPadding()
        val headerTopPadding = statusBarInset + if (landscape) 10.dp else 8.dp
        val headerHeight = maxOf(
            maxHeight * 0.065f,
            if (landscape) 84.dp else 64.dp,
        )
        val landscapeButtonTop = headerTopPadding + headerHeight + 14.dp
        val mamaButtonHeight = 50.dp
        val mamaButtonWidth = if (landscape) 144.dp else 156.dp
        val landscapeLegsTop = landscapeButtonTop + if (testMode) {
            0.dp
        } else {
            mamaButtonHeight + 10.dp
        }
        val landscapeFrontCenterY = landscapeLegsTop + LANDSCAPE_LEG_BUTTON_HEIGHT / 2
        val landscapeRearCenterY = landscapeLegsTop +
            LANDSCAPE_LEG_BUTTON_HEIGHT + 18.dp + LANDSCAPE_LEG_BUTTON_HEIGHT / 2
        val portraitLegButtonHeight = if (background.renderedHeight * 0.12f > 112.dp) {
            background.renderedHeight * 0.12f
        } else {
            112.dp
        }
        val portraitFrontButtonTop = background.offsetY +
            background.renderedHeight * layout.frontY - portraitLegButtonHeight / 2
        val mamaButtonTop = if (landscape) {
            landscapeButtonTop
        } else {
            portraitFrontButtonTop - mamaButtonHeight - 12.dp
        }
        BackgroundImage(R.drawable.ctrl)

        Surface(
            modifier = Modifier
                .align(Alignment.TopCenter)
                .padding(top = headerTopPadding)
                .width(maxWidth * 0.92f)
                .height(headerHeight),
            shape = RoundedCornerShape(20.dp),
            color = Color(0xcc173d20),
        ) {
            if (testMode) {
                Row(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = 18.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Button(
                        onClick = onHome,
                        enabled = state.gameplay.phase == GameplayPhase.PLAYING,
                        shape = RoundedCornerShape(14.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xd9fff2b8),
                            contentColor = Color(0xff21160b),
                            disabledContainerColor = Color(0x667a6e55),
                            disabledContentColor = Color(0x99fff2b8),
                        ),
                        contentPadding = androidx.compose.foundation.layout.PaddingValues(
                            horizontal = 14.dp,
                            vertical = 0.dp,
                        ),
                        modifier = Modifier.heightIn(min = 48.dp),
                    ) {
                        Text(text = GAMEPLAY_HOME_BUTTON_LABEL, fontSize = 17.sp)
                    }

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Button(
                            onClick = onToggleGameplayMode,
                            shape = RoundedCornerShape(14.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = gameplayModeButtonColor(state.gameplay.mode),
                                contentColor = Color(0xff21451f),
                            ),
                            contentPadding = androidx.compose.foundation.layout.PaddingValues(
                                horizontal = 12.dp,
                                vertical = 0.dp,
                            ),
                            modifier = Modifier.heightIn(min = 48.dp),
                        ) {
                            Text(
                                text = gameplayModeButtonLabel(state.gameplay.mode),
                                fontSize = 16.sp,
                            )
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        Button(
                            onClick = onEndGameplay,
                            enabled = state.gameplay.phase == GameplayPhase.PLAYING,
                            shape = RoundedCornerShape(14.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = Color(0xd9fff2b8),
                                contentColor = Color(0xff21160b),
                            ),
                            contentPadding = androidx.compose.foundation.layout.PaddingValues(
                                horizontal = 12.dp,
                                vertical = 0.dp,
                            ),
                            modifier = Modifier.heightIn(min = 48.dp),
                        ) {
                            Text(text = GAMEPLAY_END_BUTTON_LABEL, fontSize = 17.sp)
                        }
                    }
                }
            } else {
                Row(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = 18.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Button(
                        onClick = onHome,
                        enabled = homeEnabled,
                        shape = RoundedCornerShape(14.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xd9fff2b8),
                            contentColor = Color(0xff21160b),
                            disabledContainerColor = Color(0x667a6e55),
                            disabledContentColor = Color(0x99fff2b8),
                        ),
                        contentPadding = androidx.compose.foundation.layout.PaddingValues(
                            horizontal = 14.dp,
                            vertical = 0.dp,
                        ),
                        modifier = Modifier.heightIn(min = 48.dp),
                    ) {
                        Text(
                            text = if (state.homeRequestStatus == HomeRequestStatus.SENDING) {
                                "归位中"
                            } else {
                                GAMEPLAY_HOME_BUTTON_LABEL
                            },
                            fontSize = 17.sp,
                        )
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Button(
                            onClick = onToggleGameplayMode,
                            shape = RoundedCornerShape(14.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = gameplayModeButtonColor(state.gameplay.mode),
                                contentColor = Color(0xff21451f),
                            ),
                            contentPadding = androidx.compose.foundation.layout.PaddingValues(
                                horizontal = 12.dp,
                                vertical = 0.dp,
                            ),
                            modifier = Modifier.heightIn(min = 48.dp),
                        ) {
                            Text(
                                text = gameplayModeButtonLabel(state.gameplay.mode),
                                fontSize = 16.sp,
                            )
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        Button(
                            onClick = onEndGameplay,
                            enabled = state.gameplay.phase == GameplayPhase.PLAYING,
                            shape = RoundedCornerShape(14.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = Color(0xd9fff2b8),
                                contentColor = Color(0xff21160b),
                                disabledContainerColor = Color(0x667a6e55),
                                disabledContentColor = Color(0x99fff2b8),
                            ),
                            contentPadding = androidx.compose.foundation.layout.PaddingValues(
                                horizontal = 12.dp,
                                vertical = 0.dp,
                            ),
                            modifier = Modifier.heightIn(min = 48.dp),
                        ) { Text(text = GAMEPLAY_END_BUTTON_LABEL, fontSize = 17.sp) }
                    }
                }
            }
        }

        if (!testMode) {
            Button(
                onClick = onPlayMama,
                enabled = state.ble.phase == BlePhase.CONNECTED,
                shape = RoundedCornerShape(18.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xe6d9efb4),
                    contentColor = Color(0xff21451f),
                    disabledContainerColor = Color(0x66d9efb4),
                    disabledContentColor = Color(0x9921451f),
                ),
                contentPadding = androidx.compose.foundation.layout.PaddingValues(
                    horizontal = 22.dp,
                    vertical = 0.dp,
                ),
                modifier = Modifier
                    .width(mamaButtonWidth)
                    .height(mamaButtonHeight)
                    .offset(
                        x = background.offsetX + background.renderedWidth / 2 - mamaButtonWidth / 2,
                        y = mamaButtonTop,
                    )
                    .semantics {
                        role = Role.Button
                        contentDescription = GAMEPLAY_MAMA_BUTTON_LABEL
                    },
            ) {
                Text(text = GAMEPLAY_MAMA_BUTTON_LABEL, fontSize = 20.sp)
            }
        }

        if (!testMode) {
            val telemetry = state.ble.lastTelemetry
            Surface(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(bottom = 14.dp)
                    .width(maxWidth * 0.84f)
                    .heightIn(min = 54.dp),
                shape = RoundedCornerShape(18.dp),
                color = accelerationColor(telemetry).copy(alpha = 0.92f),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 18.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(
                        text = accelerationText(telemetry),
                        color = Color(0xff21160b),
                        fontSize = 18.sp,
                    )
                    Text(
                        text = telemetry?.let { "${it.ageMs} ms" } ?: "--",
                        color = Color(0xff21160b),
                        fontSize = 13.sp,
                    )
                }
            }
        }

        // Coordinates are normalized to ctrl.png's 1080x1920 canvas. The
        // anatomical left/right mapping follows the cow facing the top edge.
        PositionedLegButton(
            leg = Leg.FRONT_LEFT,
            pressed = state.txLegMask and Leg.FRONT_LEFT.bit != 0,
            contact = contactMask and Leg.FRONT_LEFT.bit != 0,
            touchdown = touchdownMask and Leg.FRONT_LEFT.bit != 0,
            touchdownEffectNonce = touchdownEffectNonce,
            enabled = controlsEnabled,
            centerX = layout.frontLeftX,
            centerY = layout.frontY,
            landscape = landscape,
            screenCenterY = landscapeFrontCenterY,
            viewport = background,
            onPointerDown = onPointerDown,
            onPointerUp = onPointerUp,
            onCancelLeg = onCancelLeg,
        )
        PositionedLegButton(
            leg = Leg.FRONT_RIGHT,
            pressed = state.txLegMask and Leg.FRONT_RIGHT.bit != 0,
            contact = contactMask and Leg.FRONT_RIGHT.bit != 0,
            touchdown = touchdownMask and Leg.FRONT_RIGHT.bit != 0,
            touchdownEffectNonce = touchdownEffectNonce,
            enabled = controlsEnabled,
            centerX = layout.frontRightX,
            centerY = layout.frontY,
            landscape = landscape,
            screenCenterY = landscapeFrontCenterY,
            viewport = background,
            onPointerDown = onPointerDown,
            onPointerUp = onPointerUp,
            onCancelLeg = onCancelLeg,
        )
        PositionedLegButton(
            leg = Leg.REAR_LEFT,
            pressed = state.txLegMask and Leg.REAR_LEFT.bit != 0,
            contact = contactMask and Leg.REAR_LEFT.bit != 0,
            touchdown = touchdownMask and Leg.REAR_LEFT.bit != 0,
            touchdownEffectNonce = touchdownEffectNonce,
            enabled = controlsEnabled,
            centerX = layout.rearLeftX,
            centerY = layout.rearY,
            landscape = landscape,
            screenCenterY = landscapeRearCenterY,
            viewport = background,
            onPointerDown = onPointerDown,
            onPointerUp = onPointerUp,
            onCancelLeg = onCancelLeg,
        )
        PositionedLegButton(
            leg = Leg.REAR_RIGHT,
            pressed = state.txLegMask and Leg.REAR_RIGHT.bit != 0,
            contact = contactMask and Leg.REAR_RIGHT.bit != 0,
            touchdown = touchdownMask and Leg.REAR_RIGHT.bit != 0,
            touchdownEffectNonce = touchdownEffectNonce,
            enabled = controlsEnabled,
            centerX = layout.rearRightX,
            centerY = layout.rearY,
            landscape = landscape,
            screenCenterY = landscapeRearCenterY,
            viewport = background,
            onPointerDown = onPointerDown,
            onPointerUp = onPointerUp,
            onCancelLeg = onCancelLeg,
        )

        if (showTutorial) {
            ControlTutorialDialog(
                landscape = landscape,
                mode = state.gameplay.mode,
                onToggleMode = onToggleGameplayMode,
                onDismiss = {
                    showTutorial = false
                    onTutorialComplete()
                },
            )
        }
    }
}

@Composable
private fun ResultPage(
    state: GameplayUiState,
    onRematch: () -> Unit,
    onMute: (Boolean) -> Unit,
    onExit: () -> Unit,
) {
    val result = state.result ?: return
    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        BackgroundImage(R.drawable.ctrl)
        Surface(
            modifier = Modifier
                .align(Alignment.Center)
                .fillMaxWidth(0.92f)
                .heightIn(min = 420.dp, max = maxHeight * 0.90f),
            shape = RoundedCornerShape(28.dp),
            color = Color(0xe6fff8df),
            shadowElevation = 12.dp,
        ) {
            LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 18.dp, vertical = 16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                item {
                    Text(
                        text = if (result.failed) "游戏失败" else "本局结果",
                        color = if (result.failed) Color(0xffa62d28) else Color(0xff402714),
                        fontSize = 30.sp,
                    )
                    Text(
                        text = gameplayResultModeText(state.mode, state.isSimulated),
                        color = Color(0xff765335),
                        fontSize = 15.sp,
                    )
                }
                if (result.failed) {
                    item {
                        Text(
                            text = "检测到机体倾斜超过 60°，本局失败。",
                            color = Color(0xffa62d28),
                            fontSize = 16.sp,
                        )
                    }
                }
                item {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(10.dp),
                    ) {
                        ScoreCard(
                            title = "速度分",
                            value = when {
                                result.failed -> "0"
                                !result.scoreReady -> "试跑数据"
                                else -> result.speedScore?.toString() ?: "暂无数据"
                            },
                            detail = "有效步 ${result.completedSteps} · 进度 ${result.progressPercent}%",
                            modifier = Modifier.weight(1f),
                        )
                        ScoreCard(
                            title = "人类协调分",
                            value = if (result.failed) {
                                "0"
                            } else if (!result.scoreReady) {
                                "试跑数据"
                            } else {
                                result.coordinationScore.toString()
                            },
                            detail = "有效步 ${result.completedSteps} · 控制变化 ${result.eventCount} 次",
                            modifier = Modifier.weight(1f),
                        )
                    }
                }
                item {
                    Text(
                        text = "用时 ${formatElapsed(result.elapsedMs)} · ${gameplayStepsText(result.completedSteps)}",
                        color = Color(0xff624327),
                        fontSize = 16.sp,
                    )
                }
                item {
                    AiFeedbackCard(state = state)
                }
                item {
                    Button(
                        onClick = onRematch,
                        modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xff7a4b22),
                            contentColor = Color.White,
                        ),
                    ) { Text("再来一局", fontSize = 17.sp) }
                }
                item {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        TextButton(onClick = { onMute(!state.muted) }) {
                            Text(if (state.muted) "打开音效" else "静音", color = Color(0xff315d2a))
                        }
                        TextButton(onClick = onExit) {
                            Text("返回主页", color = Color(0xff765335))
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun AiFeedbackCard(state: GameplayUiState) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 118.dp),
        shape = RoundedCornerShape(18.dp),
        color = Color(0xffe4f0d7),
    ) {
        Column(modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp)) {
            when (state.aiFeedbackStatus) {
                AiFeedbackStatus.LOADING -> Row(
                    modifier = Modifier.heightIn(min = 44.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(20.dp),
                        color = Color(0xff315d2a),
                        strokeWidth = 2.dp,
                    )
                    Spacer(modifier = Modifier.width(10.dp))
                    Text(
                        if (state.result?.failed == true) "正在生成失败评价…"
                        else "正在根据本局表现生成评价…",
                        color = Color(0xff4d633c),
                        fontSize = 14.sp,
                    )
                }

                AiFeedbackStatus.SUCCESS -> Text(
                    text = state.aiFeedback.orEmpty(),
                    modifier = Modifier.heightIn(min = 52.dp),
                    color = Color(0xff31452a),
                    fontSize = 15.sp,
                )

                AiFeedbackStatus.FALLBACK -> Text(
                    text = state.aiFeedback.orEmpty(),
                    modifier = Modifier.heightIn(min = 52.dp),
                    color = Color(0xff31452a),
                    fontSize = 15.sp,
                )

                AiFeedbackStatus.IDLE -> Text(
                    text = if (state.aiFeedbackEnabled) {
                        "本局评价暂未生成。"
                    } else {
                        "本局未生成 AI 评价。"
                    },
                    modifier = Modifier.heightIn(min = 44.dp),
                    color = Color(0xff4d633c),
                    fontSize = 14.sp,
                )
            }
            Spacer(modifier = Modifier.height(8.dp))
            Text(
                text = "游戏评价由AI生成。",
                color = Color(0xff8a8a8a),
                fontSize = 11.sp,
            )
        }
    }
}

@Composable
private fun ScoreCard(
    title: String,
    value: String,
    detail: String,
    modifier: Modifier = Modifier,
) {
    Surface(
        modifier = modifier.heightIn(min = 100.dp),
        shape = RoundedCornerShape(18.dp),
        color = Color(0xfffff1c4),
    ) {
        Column(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(title, color = Color(0xff765335), fontSize = 15.sp)
            Text(value, color = Color(0xff402714), fontSize = 27.sp)
            Text(detail, color = Color(0xff765335), fontSize = 12.sp)
        }
    }
}

private fun formatElapsed(milliseconds: Long): String {
    val seconds = (milliseconds / 1_000L).coerceAtLeast(0L)
    return "%02d:%02d".format(seconds / 60L, seconds % 60L)
}

@Composable
private fun ControlTutorialDialog(
    landscape: Boolean,
    mode: GameplayMode,
    onToggleMode: () -> Unit,
    onDismiss: () -> Unit,
) {
    Dialog(onDismissRequest = onDismiss) {
        Surface(
            modifier = Modifier
                .fillMaxWidth(if (landscape) 0.98f else 0.92f)
                .clip(RoundedCornerShape(30.dp)),
            color = Color(0xfffff8df),
            shadowElevation = 16.dp,
        ) {
            Column(
                modifier = Modifier.padding(if (landscape) 14.dp else 20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                if (landscape) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        TutorialIntro(
                            landscape = true,
                            modifier = Modifier.weight(1.2f),
                        )
                        TutorialActions(
                            landscape = true,
                            mode = mode,
                            onToggleMode = onToggleMode,
                            onDismiss = onDismiss,
                            modifier = Modifier.weight(0.8f),
                        )
                    }
                } else {
                    TutorialIntro(
                        landscape = false,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Spacer(modifier = Modifier.height(14.dp))
                    TutorialActions(
                        landscape = false,
                        mode = mode,
                        onToggleMode = onToggleMode,
                        onDismiss = onDismiss,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        }
    }
}

@Composable
private fun TutorialIntro(
    landscape: Boolean,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(
            text = "先学会一抬一落",
            color = Color(0xff402714),
            fontSize = if (landscape) 26.sp else 30.sp,
        )
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = "长按抬脚，松开落脚，和队友配合前进",
            color = Color(0xff765335),
            fontSize = if (landscape) 14.sp else 16.sp,
        )
        Spacer(modifier = Modifier.height(if (landscape) 8.dp else 12.dp))
        Image(
            painter = painterResource(R.drawable.control_tutorial),
            contentDescription = "抬脚和放脚示意图",
            contentScale = ContentScale.Fit,
            modifier = Modifier
                .fillMaxWidth()
                .height(178.dp)
                .clip(RoundedCornerShape(20.dp)),
        )
    }
}

@Composable
private fun TutorialActions(
    landscape: Boolean,
    mode: GameplayMode,
    onToggleMode: () -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(if (landscape) 10.dp else 0.dp),
    ) {
        val modeButtonColor = if (mode == GameplayMode.SOLO) {
            Color(0xff315d2a)
        } else {
            Color(0xffb2761d)
        }
        Button(
            onClick = onToggleMode,
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = if (landscape) 56.dp else 58.dp),
            shape = RoundedCornerShape(16.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = modeButtonColor,
                contentColor = Color.White,
            ),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(
                horizontal = 14.dp,
                vertical = 8.dp,
            ),
        ) {
            Text(
                text = if (mode == GameplayMode.SOLO) {
                    "切换到双人模式  ·  A：FL/RL  B：FR/RR"
                } else {
                    "切换回单人模式"
                },
                fontSize = if (landscape) 14.sp else 16.sp,
            )
        }
        if (landscape) {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                TutorialStep(
                    number = "1",
                    title = "抬脚",
                    detail = "按住对应腿",
                    accent = Color(0xff315d2a),
                    modifier = Modifier.fillMaxWidth(),
                )
                TutorialStep(
                    number = "2",
                    title = "放脚",
                    detail = "松开即可落脚",
                    accent = Color(0xffb2761d),
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        } else {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                TutorialStep(
                    number = "1",
                    title = "抬脚",
                    detail = "按住对应腿",
                    accent = Color(0xff315d2a),
                    modifier = Modifier.weight(1f),
                )
                TutorialStep(
                    number = "2",
                    title = "放脚",
                    detail = "松开即可落脚",
                    accent = Color(0xffb2761d),
                    modifier = Modifier.weight(1f),
                )
            }
        }
        if (!landscape) {
            Spacer(modifier = Modifier.height(16.dp))
        }
        Button(
            onClick = onDismiss,
            shape = RoundedCornerShape(18.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = Color(0xff315d2a),
                contentColor = Color.White,
            ),
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = if (landscape) 56.dp else 52.dp),
        ) {
            Text(text = "知道了，开始玩", fontSize = if (landscape) 17.sp else 20.sp)
        }
    }
}

@Composable
private fun TutorialStep(
    number: String,
    title: String,
    detail: String,
    accent: Color,
    modifier: Modifier = Modifier,
) {
    Surface(
        modifier = modifier.heightIn(min = 64.dp),
        shape = RoundedCornerShape(16.dp),
        color = Color(0xfffff1c4),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 9.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Surface(
                shape = RoundedCornerShape(99.dp),
                color = accent,
                modifier = Modifier.size(30.dp),
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Text(text = number, color = Color.White, fontSize = 17.sp)
                }
            }
            Spacer(modifier = Modifier.width(8.dp))
            Column {
                Text(text = title, color = Color(0xff402714), fontSize = 19.sp)
                Text(text = detail, color = Color(0xff765335), fontSize = 13.sp)
            }
        }
    }
}

@Composable
private fun PositionedLegButton(
    leg: Leg,
    pressed: Boolean,
    contact: Boolean,
    touchdown: Boolean,
    touchdownEffectNonce: Int,
    enabled: Boolean,
    centerX: Float,
    centerY: Float,
    landscape: Boolean,
    screenCenterY: Dp,
    viewport: CropGeometry,
    onPointerDown: (Long, Leg) -> Unit,
    onPointerUp: (Long) -> Unit,
    onCancelLeg: (Leg) -> Unit,
) {
    val buttonWidth = if (landscape) {
        (viewport.renderedWidth * 0.22f).coerceAtLeast(116.dp)
    } else if (viewport.renderedWidth * 0.30f > 124.dp) {
        viewport.renderedWidth * 0.30f
    } else {
        124.dp
    }
    val buttonHeight = if (landscape) {
        LANDSCAPE_LEG_BUTTON_HEIGHT
    } else if (viewport.renderedHeight * 0.12f > 112.dp) {
        viewport.renderedHeight * 0.12f
    } else {
        112.dp
    }
    val shape = RoundedCornerShape(34.dp)
    val fill = when {
        !enabled -> Color(0x55909080)
        contact -> Color(0xd9b8efb0)
        pressed -> Color(0xe6fff0a8)
        else -> Color(0x88fff0a8)
    }
    val outline = when {
        contact -> Color(0xff315d2a)
        pressed -> Color.White
        enabled -> Color(0xbffff0a8)
        else -> Color(0x55909080)
    }

    Box(
        modifier = Modifier
            .width(buttonWidth)
            .height(buttonHeight)
            .offset(
                x = viewport.offsetX + viewport.renderedWidth * centerX - buttonWidth / 2,
                y = if (landscape) {
                    screenCenterY - buttonHeight / 2
                } else {
                    viewport.offsetY + viewport.renderedHeight * centerY - buttonHeight / 2
                },
            ),
        contentAlignment = Alignment.TopCenter,
    ) {
        if (touchdown) {
            StepCelebration(trigger = touchdownEffectNonce)
        }
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(shape)
                .background(fill, shape)
                .border(3.dp, outline, shape)
                .semantics {
                    role = Role.Button
                    contentDescription = legControlName(leg)
                }
                .pointerInput(enabled, leg) {
                    if (!enabled) return@pointerInput
                    try {
                        awaitEachGesture {
                            do {
                                val event = awaitPointerEvent(PointerEventPass.Main)
                                event.changes.forEach { change ->
                                    if (change.pressed && !change.previousPressed) {
                                        onPointerDown(change.id.value, leg)
                                        change.consume()
                                    } else if (!change.pressed && change.previousPressed) {
                                        onPointerUp(change.id.value)
                                        change.consume()
                                    }
                                }
                            } while (event.changes.any { it.pressed })
                        }
                    } finally {
                        onCancelLeg(leg)
                    }
                },
            contentAlignment = Alignment.Center,
        ) {
            if (touchdown) {
                TouchdownBurst(trigger = touchdownEffectNonce)
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(
                    text = leg.shortLabel,
                    color = Color(0xff25170d),
                    fontSize = 31.sp,
                )
                Text(
                    text = when {
                        pressed -> "已按下"
                        else -> legControlName(leg)
                    },
                    color = Color(0xff25170d),
                    fontSize = 17.sp,
                )
            }
        }
    }
}

internal fun stepCelebrationAlpha(progress: Float): Float = when {
    progress <= 0.22f -> (progress / 0.22f).coerceIn(0f, 1f)
    else -> ((1f - progress) / 0.78f).coerceIn(0f, 1f)
}

@Composable
private fun BoxScope.StepCelebration(trigger: Int) {
    val progress = remember { Animatable(0f) }
    LaunchedEffect(trigger) {
        progress.snapTo(0f)
        progress.animateTo(1f, animationSpec = tween(durationMillis = 900))
    }
    Text(
        text = "稳稳地踏出了一步！",
        color = Color(0xff315d2a),
        fontSize = 12.sp,
        maxLines = 1,
        modifier = Modifier
            .align(Alignment.TopCenter)
            .offset(y = -30.dp - 26.dp * progress.value)
            .graphicsLayer { alpha = stepCelebrationAlpha(progress.value) },
    )
}

@Composable
private fun TouchdownBurst(trigger: Int) {
    val progress = remember { Animatable(1f) }
    LaunchedEffect(trigger) {
        progress.snapTo(0f)
        progress.animateTo(1f, animationSpec = tween(durationMillis = 520))
    }
    Canvas(modifier = Modifier.fillMaxSize()) {
        val p = progress.value
        val center = androidx.compose.ui.geometry.Offset(size.width / 2f, size.height / 2f)
        val baseRadius = size.minDimension * 0.13f
        val radius = baseRadius + size.minDimension * 0.46f * p
        val alpha = (1f - p).coerceAtLeast(0f)

        drawCircle(
            color = Color.White.copy(alpha = alpha * 0.90f),
            radius = radius,
            center = center,
            style = androidx.compose.ui.graphics.drawscope.Stroke(
                width = size.minDimension * 0.025f,
            ),
        )
        drawCircle(
            color = Color(0xfffff3a6).copy(alpha = alpha * 0.55f),
            radius = baseRadius * (1f + p * 0.8f),
            center = center,
        )
        repeat(8) { index ->
            val angle = index * (Math.PI / 4.0)
            val inner = radius * 0.72f
            val outer = radius * (1.02f + (index % 2) * 0.12f)
            drawLine(
                color = Color.White.copy(alpha = alpha * 0.82f),
                start = androidx.compose.ui.geometry.Offset(
                    center.x + cos(angle).toFloat() * inner,
                    center.y + sin(angle).toFloat() * inner,
                ),
                end = androidx.compose.ui.geometry.Offset(
                    center.x + cos(angle).toFloat() * outer,
                    center.y + sin(angle).toFloat() * outer,
                ),
                strokeWidth = size.minDimension * 0.018f,
                cap = StrokeCap.Round,
            )
        }
    }
}

private fun legControlName(leg: Leg): String = when (leg) {
    Leg.FRONT_LEFT -> "前左腿"
    Leg.FRONT_RIGHT -> "前右腿"
    Leg.REAR_LEFT -> "后左腿"
    Leg.REAR_RIGHT -> "后右腿"
}
