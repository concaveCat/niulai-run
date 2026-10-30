package io.niulai.controller

import android.app.Application
import android.util.Log

/**
 * Emits app-owned crash reports before the platform's normal crash handler
 * terminates the process. The previous handler is always invoked afterward.
 */
class NiulaiApplication : Application() {
    companion object {
        const val LOG_TAG = "Niulai"
    }

    override fun onCreate() {
        super.onCreate()
        val previousHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            Log.e(
                LOG_TAG,
                "APP_UNCAUGHT_EXCEPTION thread=${thread.name}; forwarding to Android runtime",
                throwable,
            )
            previousHandler?.uncaughtException(thread, throwable)
        }
        Log.i(LOG_TAG, "APP_CREATED package=$packageName")
    }
}
