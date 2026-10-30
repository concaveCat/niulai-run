package io.niulai.controller.ui

import io.niulai.controller.ble.BlePhase
import io.niulai.controller.gameplay.GameplayMode
import io.niulai.controller.protocol.StatusPacket
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

class ScreenRoutingTest {
    @Test
    fun connectedDeviceOpensControlPage() {
        assertEquals(
            AppScreen.CTRL,
            screenAfterBlePhase(AppScreen.CONNECT, BlePhase.CONNECTED),
        )
    }

    @Test
    fun physicalResultRemainsOnConnectedControlRoute() {
        assertEquals(AppScreen.CTRL, gameplaySessionScreen(isSimulated = false))
        assertEquals(AppScreen.TEST, gameplaySessionScreen(isSimulated = true))
    }

    @Test
    fun contactStatusNamesEveryDebouncedTouchdownLeg() {
        assertEquals("已连接 · 等待 KW10 落脚", contactStatusText(0x00))
        assertEquals("已连接 · 完美触碰：FL/RR", contactStatusText(0x09))
        assertEquals("已连接 · 完美触碰：FL/FR/RL/RR", contactStatusText(0x0f))
    }

    @Test
    fun touchdownEffectOnlyUsesContactRisingEdges() {
        assertEquals(0x05, contactRisingMask(previousMask = 0x02, currentMask = 0x07))
        assertEquals(0x00, contactRisingMask(previousMask = 0x0f, currentMask = 0x0f))
        assertEquals(0x01, contactRisingMask(previousMask = 0x10, currentMask = 0x01))
    }

    @Test
    fun failsafeTakesPriorityOverContactStatusAndDisablesControls() {
        val failsafe = StatusPacket(sequence = 9, appliedLegMask = 0, flags = 0x01)

        assertEquals("安全停止 · FAILSAFE", controllerStatusText(failsafe))
        assertEquals("STATUS seq=9 · applied=0x00", statusDiagnosticText(failsafe))
        assertTrue(!bleControlsEnabled(BlePhase.CONNECTED, failsafeActive = true))
    }

    @Test
    fun fallStatusDisablesControlsAndShowsFailure() {
        val fallen = StatusPacket(sequence = 10, appliedLegMask = 0, flags = 0x20)

        assertEquals("摔倒保护 · 本局失败", controllerStatusText(fallen))
        assertTrue(!bleControlsEnabled(BlePhase.CONNECTED, failsafeActive = false,
            fallDetected = true))
    }

    @Test
    fun healthyStatusShowsContactAndAppliedMaskDiagnostics() {
        val status = StatusPacket(sequence = 255, appliedLegMask = 0x08, flags = 0x0a)

        assertEquals("已连接 · 完美触碰：FL/RL", controllerStatusText(status))
        assertEquals("STATUS seq=255 · applied=0x08", statusDiagnosticText(status))
        assertTrue(bleControlsEnabled(BlePhase.CONNECTED, failsafeActive = false))
    }

    @Test
    fun connectedStatusContactMaskIsIndependentOfGameplayMode() {
        val status = StatusPacket(sequence = 4, appliedLegMask = 0, flags = 0x10)

        assertEquals(0x08, contactMaskForControl(BlePhase.CONNECTED, status))
        assertEquals(0, contactMaskForControl(BlePhase.DISCONNECTED, status))
    }

    @Test
    fun disconnectingFromControlReturnsToConnectPage() {
        assertEquals(
            AppScreen.CONNECT,
            screenAfterBlePhase(AppScreen.CTRL, BlePhase.DISCONNECTED),
        )
    }

    @Test
    fun mainPageStaysVisibleWhileNotConnected() {
        assertEquals(
            AppScreen.MAIN,
            screenAfterBlePhase(AppScreen.MAIN, BlePhase.DISCONNECTED),
        )
    }

    @Test
    fun testPageStaysOpenWithoutBleConnection() {
        assertEquals(
            AppScreen.TEST,
            screenAfterBlePhase(AppScreen.TEST, BlePhase.DISCONNECTED),
        )
    }

    @Test
    fun settingsPageStaysOpenWithoutBleConnection() {
        assertEquals(
            AppScreen.SETTINGS,
            screenAfterBlePhase(AppScreen.SETTINGS, BlePhase.DISCONNECTED),
        )
    }

    @Test
    fun physicalLeftAndRightButtonsMatchSemanticSides() {
        val portrait = controlLayout(isLandscape = false)
        val layout = controlLayout(isLandscape = true)

        assertTrue(portrait.frontLeftX < portrait.frontRightX)
        assertTrue(portrait.rearLeftX < portrait.rearRightX)
        assertEquals(0.43f, layout.frontY, 0.001f)
        assertEquals(0.57f, layout.rearY, 0.001f)
        assertTrue(layout.frontLeftX < layout.frontRightX)
        assertTrue(layout.rearLeftX < layout.rearRightX)
        assertTrue(layout.frontRightX - layout.frontLeftX > 0.39f)
        assertTrue(layout.rearRightX - layout.rearLeftX > 0.39f)
    }

    @Test
    fun stepCelebrationFadesInThenOut() {
        assertEquals(0f, stepCelebrationAlpha(0f), 0.001f)
        assertEquals(1f, stepCelebrationAlpha(0.22f), 0.001f)
        assertTrue(stepCelebrationAlpha(0.50f) in 0f..1f)
        assertEquals(0f, stepCelebrationAlpha(1f), 0.001f)
    }

    @Test
    fun gameplayEndButtonUsesExplicitGameOverLabel() {
        assertEquals("结束游戏", GAMEPLAY_END_BUTTON_LABEL)
    }

    @Test
    fun gameplayHomeButtonUsesExplicitReturnToRestLabel() {
        assertEquals("归位", GAMEPLAY_HOME_BUTTON_LABEL)
    }

    @Test
    fun resultIdentifiesPhysicalInputSeparatelyFromTestData() {
        assertEquals(
            "单人模式 · 模拟数据",
            gameplayResultModeText(GameplayMode.SOLO, isSimulated = true),
        )
        assertEquals(
            "双人模式 · 实体输入记录",
            gameplayResultModeText(GameplayMode.SPLIT, isSimulated = false),
        )
    }

    @Test
    fun resultUsesPlainChineseStepCount() {
        assertEquals("迈出了7步", gameplayStepsText(7))
        assertEquals("迈出了0步", gameplayStepsText(-1))
    }

    @Test
    fun rematchReturnsToTheMatchingGameplaySurface() {
        assertEquals(AppScreen.TEST, gameplaySessionScreen(isSimulated = true))
        assertEquals(AppScreen.CTRL, gameplaySessionScreen(isSimulated = false))
    }

    @Test
    fun resultReturnHomeAlwaysTargetsMain() {
        assertEquals(AppScreen.MAIN, gameplayResultExitScreen())
    }

    @Test
    fun modeButtonShowsTheCurrentModeAndKeepsLightSoloColor() {
        assertEquals("单人模式", gameplayModeButtonLabel(GameplayMode.SOLO))
        assertEquals("双人模式", gameplayModeButtonLabel(GameplayMode.SPLIT))
        assertEquals(Color(0xffeef8dc), gameplayModeButtonColor(GameplayMode.SOLO))
    }

    @Test
    fun gameplayMamaButtonUsesVoiceLabel() {
        assertEquals("牛来", GAMEPLAY_MAMA_BUTTON_LABEL)
    }

    @Test
    fun portraitAndLandscapeLayoutsKeepDistinctFrontAndRearRows() {
        val portrait = controlLayout(isLandscape = false)
        val landscape = controlLayout(isLandscape = true)

        assertTrue(portrait.rearY > portrait.frontY)
        assertTrue(landscape.rearY > landscape.frontY)
        assertTrue(landscape.rearRightX - landscape.rearLeftX > 0.39f)
    }

    @Test
    fun narrowViewportCropsImageInsteadOfCompressingIt() {
        val geometry = cropGeometry(360.dp, 800.dp, 9f / 16f)

        assertEquals(800.dp, geometry.renderedHeight)
        assertTrue(geometry.renderedWidth > 360.dp)
        assertTrue(geometry.offsetX < 0.dp)
        assertEquals(9f / 16f, geometry.renderedWidth / geometry.renderedHeight, 0.001f)
    }
}
