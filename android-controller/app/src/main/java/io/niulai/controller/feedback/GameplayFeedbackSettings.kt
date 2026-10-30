package io.niulai.controller.feedback

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import io.niulai.controller.BuildConfig
import java.nio.charset.StandardCharsets
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

data class GameplayFeedbackSettings(
    val apiUrl: String = BuildConfig.GAMEPLAY_FEEDBACK_BASE_URL,
    val apiKey: String = "",
    val model: String = BuildConfig.GAMEPLAY_FEEDBACK_MODEL,
    val useProxy: Boolean = false,
)

interface FeedbackSettingsStore {
    fun read(): GameplayFeedbackSettings
    fun save(settings: GameplayFeedbackSettings): Boolean
}

/** Stores the endpoint openly and protects the API key with the Android Keystore. */
class GameplayFeedbackSettingsStore(context: Context) : FeedbackSettingsStore {
    private val preferences = context.applicationContext.getSharedPreferences(
        PREFERENCES_NAME,
        Context.MODE_PRIVATE,
    )

    override fun read(): GameplayFeedbackSettings {
        val apiUrl = preferences.getString(API_URL_KEY, null)?.trim()
            .takeUnless { it.isNullOrEmpty() }
            ?: BuildConfig.GAMEPLAY_FEEDBACK_BASE_URL
        val apiKey = decrypt(
            preferences.getString(API_KEY_CIPHERTEXT_KEY, null),
            preferences.getString(API_KEY_IV_KEY, null),
        ).orEmpty()
        val useProxy = if (preferences.contains(USE_PROXY_KEY)) {
            preferences.getBoolean(USE_PROXY_KEY, false)
        } else {
            // Migrate the previous default configuration, where an empty key
            // implicitly meant the emulator's local proxy.
            apiKey.isBlank() && isLegacyLocalProxyUrl(apiUrl)
        }
        return GameplayFeedbackSettings(
            apiUrl = apiUrl,
            apiKey = apiKey,
            model = preferences.getString(MODEL_KEY, null)?.trim()
            .takeUnless { it.isNullOrEmpty() }
            ?: BuildConfig.GAMEPLAY_FEEDBACK_MODEL,
            useProxy = useProxy,
        )
    }

    override fun save(settings: GameplayFeedbackSettings): Boolean {
        val editor = preferences.edit()
            .putString(API_URL_KEY, settings.apiUrl.trim())
            .putString(MODEL_KEY, settings.model.trim())
            .putBoolean(USE_PROXY_KEY, settings.useProxy)

        if (settings.apiKey.isBlank()) {
            editor.remove(API_KEY_CIPHERTEXT_KEY)
            editor.remove(API_KEY_IV_KEY)
        } else {
            val encrypted = runCatching { encrypt(settings.apiKey.trim()) }.getOrNull()
                ?: return false
            editor.putString(API_KEY_CIPHERTEXT_KEY, encrypted.ciphertext)
            editor.putString(API_KEY_IV_KEY, encrypted.iv)
        }
        return editor.commit()
    }

    private fun encrypt(value: String): EncryptedValue {
        val cipher = Cipher.getInstance(TRANSFORMATION)
        cipher.init(Cipher.ENCRYPT_MODE, key())
        return EncryptedValue(
            ciphertext = cipher.doFinal(value.toByteArray(StandardCharsets.UTF_8)).encodeBase64(),
            iv = cipher.iv.encodeBase64(),
        )
    }

    private fun decrypt(ciphertext: String?, iv: String?): String? {
        if (ciphertext.isNullOrBlank() || iv.isNullOrBlank()) return null
        return runCatching {
            val cipher = Cipher.getInstance(TRANSFORMATION)
            cipher.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(GCM_TAG_BITS, iv.decodeBase64()))
            String(cipher.doFinal(ciphertext.decodeBase64()), StandardCharsets.UTF_8)
        }.getOrNull()
    }

    private fun key(): SecretKey {
        val keyStore = KeyStore.getInstance(KEYSTORE_PROVIDER).apply { load(null) }
        (keyStore.getKey(KEY_ALIAS, null) as? SecretKey)?.let { return it }

        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE_PROVIDER).apply {
            init(
                KeyGenParameterSpec.Builder(
                    KEY_ALIAS,
                    KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT,
                )
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256)
                    .build(),
            )
        }.generateKey()
    }

    private data class EncryptedValue(
        val ciphertext: String,
        val iv: String,
    )

    private fun ByteArray.encodeBase64(): String = Base64.encodeToString(this, Base64.NO_WRAP)

    private fun String.decodeBase64(): ByteArray = Base64.decode(this, Base64.NO_WRAP)

    private companion object {
        const val PREFERENCES_NAME = "gameplay_feedback"
        const val API_URL_KEY = "api_url"
        const val API_KEY_CIPHERTEXT_KEY = "api_key_ciphertext"
        const val API_KEY_IV_KEY = "api_key_iv"
        const val MODEL_KEY = "model"
        const val USE_PROXY_KEY = "use_proxy"
        const val KEY_ALIAS = "niulai_gameplay_feedback_key"
        const val KEYSTORE_PROVIDER = "AndroidKeyStore"
        const val TRANSFORMATION = "AES/GCM/NoPadding"
        const val GCM_TAG_BITS = 128
    }
}

private fun isLegacyLocalProxyUrl(value: String): Boolean {
    val normalized = value.trim().trimEnd('/').lowercase()
    return normalized == "http://10.0.2.2:8787" ||
        normalized == "http://localhost:8787" ||
        normalized == "http://127.0.0.1:8787"
}
