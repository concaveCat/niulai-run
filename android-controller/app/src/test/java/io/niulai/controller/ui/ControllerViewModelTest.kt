package io.niulai.controller.ui

import android.app.Application
import org.junit.Assert.assertNotNull
import org.junit.Test

class ControllerViewModelTest {
    @Test
    fun androidViewModelFactoryConstructorIsPublic() {
        assertNotNull(ControllerViewModel::class.java.getConstructor(Application::class.java))
    }
}
