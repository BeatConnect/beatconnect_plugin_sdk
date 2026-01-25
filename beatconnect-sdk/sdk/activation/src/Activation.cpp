/**
 * BeatConnect Activation SDK - Implementation
 *
 * Uses JUCE for HTTP client and JSON parsing since plugins already have it.
 * Falls back to system APIs if JUCE is not available.
 *
 * IMPORTANT: This is an instance-based implementation. Each PluginProcessor
 * should own its own Activation instance to avoid conflicts when multiple
 * plugins are loaded in the same DAW process.
 *
 * DO NOT use static members or singletons - they cause issues when multiple
 * plugin instances share the same address space.
 */

#include "beatconnect/Activation.h"
#include "beatconnect/MachineId.h"

#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>

#if __has_include(<juce_core/juce_core.h>)
    #define BEATCONNECT_USE_JUCE 1
    #include <juce_core/juce_core.h>
#else
    #define BEATCONNECT_USE_JUCE 0
    // TODO: Implement standalone HTTP client
    #error "Currently requires JUCE. Standalone implementation coming soon."
#endif

namespace beatconnect {

// ==============================================================================
// Status String Conversion
// ==============================================================================

const char* activationStatusToString(ActivationStatus status) {
    switch (status) {
        case ActivationStatus::Valid:         return "valid";
        case ActivationStatus::Invalid:       return "invalid";
        case ActivationStatus::Revoked:       return "revoked";
        case ActivationStatus::MaxReached:    return "max_reached";
        case ActivationStatus::NetworkError:  return "network_error";
        case ActivationStatus::ServerError:   return "server_error";
        case ActivationStatus::NotConfigured: return "not_configured";
        case ActivationStatus::AlreadyActive: return "already_active";
        case ActivationStatus::NotActivated:  return "not_activated";
    }
    return "unknown";
}

namespace {
    std::string getTimestamp() {
#if BEATCONNECT_USE_JUCE
        return juce::Time::getCurrentTime().toString(false, true, true, true).toStdString();
#else
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time));
        return buf;
#endif
    }
}

// ==============================================================================
// Implementation Class
// ==============================================================================

class Activation::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    // Debug logging - instance-based, no static state
    void setDebugEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex);
        debugEnabled = enabled;

        if (enabled && !debugLogPath.empty()) {
            // Clear log on enable
            std::ofstream ofs(debugLogPath, std::ios::trunc);
            ofs << "[" << getTimestamp() << "] === Debug logging enabled ===" << std::endl;
        }
    }

    bool isDebugEnabled() const {
        std::lock_guard<std::mutex> lock(mutex);
        return debugEnabled;
    }

    std::string getDebugLogPath() const {
        std::lock_guard<std::mutex> lock(mutex);
        return debugLogPath;
    }

    void setDebugCallback(Activation::DebugCallback callback) {
        std::lock_guard<std::mutex> lock(mutex);
        debugCallback = callback;
    }

    void debug(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex);

        if (!debugEnabled) return;

        // Write to log file
        if (!debugLogPath.empty()) {
            std::ofstream ofs(debugLogPath, std::ios::app);
            if (ofs.is_open()) {
                ofs << "[" << getTimestamp() << "] " << msg << std::endl;
            }
        }

#if BEATCONNECT_USE_JUCE
        DBG(juce::String("[BeatConnect] " + msg));
#endif

        // Call callback if set
        if (debugCallback) {
            debugCallback("[BeatConnect] " + msg);
        }
    }

    void configure(const ActivationConfig& cfg) {
        this->config = cfg;
        configured = true;

        // Set default state path if not provided
        if (config.statePath.empty()) {
#if BEATCONNECT_USE_JUCE
            auto appData = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory);

            statePath = appData.getChildFile("BeatConnect")
                              .getChildFile(config.pluginId)
                              .getChildFile("activation.json")
                              .getFullPathName().toStdString();

            // Also set debug log path
            debugLogPath = appData.getChildFile("BeatConnect")
                                 .getChildFile(config.pluginId)
                                 .getChildFile("debug.log")
                                 .getFullPathName().toStdString();

            // Ensure directory exists
            juce::File(statePath).getParentDirectory().createDirectory();
#else
            statePath = "activation.json";
            debugLogPath = "debug.log";
#endif
        } else {
            statePath = config.statePath;
        }

        loadState();
    }

    bool isConfigured() const {
        return configured;
    }

    bool isActivated() const {
        std::lock_guard<std::mutex> lock(mutex);
        return activated && activationInfo.isValid;
    }

    std::optional<ActivationInfo> getActivationInfo() const {
        std::lock_guard<std::mutex> lock(mutex);
        if (activated) {
            return activationInfo;
        }
        return std::nullopt;
    }

    ActivationStatus activate(const std::string& code) {
        if (!configured) {
            debug("activate: Not configured");
            return ActivationStatus::NotConfigured;
        }

        auto machineId = MachineId::generate();
        debug("activate: machineId = " + machineId);

        // Build request
#if BEATCONNECT_USE_JUCE
        // apiBaseUrl is just the Supabase URL (e.g., https://xxx.supabase.co)
        // We need to add /functions/v1/ to reach edge functions
        juce::String urlStr = juce::String(config.apiBaseUrl) + "/functions/v1/plugin-activation/activate";
        debug("activate: URL = " + urlStr.toStdString());
        juce::URL url(urlStr);

        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("code", juce::String(code));
        body->setProperty("plugin_id", juce::String(config.pluginId));
        body->setProperty("machine_id", juce::String(machineId));

        auto jsonBody = juce::JSON::toString(juce::var(body.get()));
        debug("activate: Request body = " + jsonBody.toStdString());

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inPostData);

        // Build headers with Supabase authentication
        juce::String headers = "Content-Type: application/json\r\n";
        if (!config.supabaseKey.empty()) {
            headers += "apikey: " + juce::String(config.supabaseKey) + "\r\n";
            headers += "Authorization: Bearer " + juce::String(config.supabaseKey) + "\r\n";
            debug("activate: Using supabaseKey (length=" + std::to_string(config.supabaseKey.length()) + ")");
        } else {
            debug("activate: WARNING - No supabaseKey configured!");
        }
        options.withExtraHeaders(headers);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);
        options.withNumRedirectsToFollow(0);

        url = url.withPOSTData(jsonBody);

        debug("activate: Creating input stream...");
        auto stream = url.createInputStream(options);
        if (!stream) {
            debug("activate: FAILED - createInputStream returned null (NetworkError)");
            return ActivationStatus::NetworkError;
        }

        debug("activate: Reading response...");
        auto response = stream->readEntireStreamAsString();
        debug("activate: Response (length=" + std::to_string(response.length()) + "): " + response.toStdString());

        auto json = juce::JSON::parse(response);

        if (json.isVoid()) {
            debug("activate: FAILED - JSON parse returned void (ServerError)");
            return ActivationStatus::ServerError;
        }

        auto* obj = json.getDynamicObject();
        if (!obj) {
            debug("activate: FAILED - JSON is not an object (ServerError)");
            return ActivationStatus::ServerError;
        }

        // Check for error
        if (obj->hasProperty("error")) {
            auto error = obj->getProperty("error").toString().toStdString();
            debug("activate: Server returned error: " + error);
            if (error.find("Invalid") != std::string::npos) {
                return ActivationStatus::Invalid;
            }
            if (error.find("revoked") != std::string::npos) {
                return ActivationStatus::Revoked;
            }
            if (error.find("maximum") != std::string::npos ||
                error.find("limit") != std::string::npos) {
                return ActivationStatus::MaxReached;
            }
            return ActivationStatus::ServerError;
        }

        // Success - update state
        {
            std::lock_guard<std::mutex> lock(mutex);
            activationInfo.activationCode = code;
            activationInfo.machineId = machineId;
            activationInfo.isValid = true;

            // Server returns "activated_at" or we generate it locally
            if (obj->hasProperty("activated_at")) {
                activationInfo.activatedAt = obj->getProperty("activated_at")
                    .toString().toStdString();
            } else {
                // Generate timestamp locally (server doesn't always return it)
                activationInfo.activatedAt = juce::Time::getCurrentTime()
                    .toISO8601(true).toStdString();
            }

            // Server returns "activations" (not "current_activations")
            if (obj->hasProperty("activations")) {
                activationInfo.currentActivations = static_cast<int>(
                    obj->getProperty("activations"));
            } else if (obj->hasProperty("current_activations")) {
                activationInfo.currentActivations = static_cast<int>(
                    obj->getProperty("current_activations"));
            }

            if (obj->hasProperty("max_activations")) {
                activationInfo.maxActivations = static_cast<int>(
                    obj->getProperty("max_activations"));
            }

            activated = true;
        }

        saveState();
        return ActivationStatus::Valid;
#else
        return ActivationStatus::NotConfigured;
#endif
    }

    ActivationStatus deactivate() {
        if (!configured) {
            return ActivationStatus::NotConfigured;
        }

        std::string code;
        std::string machineId;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!activated) {
                return ActivationStatus::NotActivated;
            }
            code = activationInfo.activationCode;
            machineId = activationInfo.machineId;
        }

#if BEATCONNECT_USE_JUCE
        juce::URL url(juce::String(config.apiBaseUrl) + "/functions/v1/plugin-activation/deactivate");

        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("code", juce::String(code));
        body->setProperty("plugin_id", juce::String(config.pluginId));
        body->setProperty("machine_id", juce::String(machineId));

        auto jsonBody = juce::JSON::toString(juce::var(body.get()));

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inPostData);

        // Build headers with Supabase authentication
        juce::String headers = "Content-Type: application/json\r\n";
        if (!config.supabaseKey.empty()) {
            headers += "apikey: " + juce::String(config.supabaseKey) + "\r\n";
            headers += "Authorization: Bearer " + juce::String(config.supabaseKey) + "\r\n";
        }
        options.withExtraHeaders(headers);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);

        url = url.withPOSTData(jsonBody);

        auto stream = url.createInputStream(options);
        if (!stream) {
            return ActivationStatus::NetworkError;
        }

        auto response = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(response);

        if (json.isVoid() || !json.getDynamicObject()) {
            return ActivationStatus::ServerError;
        }

        auto* obj = json.getDynamicObject();
        if (obj->hasProperty("error")) {
            return ActivationStatus::ServerError;
        }

        // Success - clear local state
        {
            std::lock_guard<std::mutex> lock(mutex);
            activated = false;
            activationInfo = ActivationInfo{};
        }

        clearState();
        return ActivationStatus::Valid;
#else
        return ActivationStatus::NotConfigured;
#endif
    }

    ActivationStatus validate() {
        if (!configured) {
            return ActivationStatus::NotConfigured;
        }

        std::string code;
        std::string machineId;

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!activated) {
                return ActivationStatus::NotActivated;
            }
            code = activationInfo.activationCode;
            machineId = activationInfo.machineId;
        }

#if BEATCONNECT_USE_JUCE
        juce::URL url(juce::String(config.apiBaseUrl) + "/functions/v1/plugin-activation/validate");

        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("code", juce::String(code));
        body->setProperty("plugin_id", juce::String(config.pluginId));
        body->setProperty("machine_id", juce::String(machineId));

        auto jsonBody = juce::JSON::toString(juce::var(body.get()));

        juce::URL::InputStreamOptions options(
            juce::URL::ParameterHandling::inPostData);

        // Build headers with Supabase authentication
        juce::String headers = "Content-Type: application/json\r\n";
        if (!config.supabaseKey.empty()) {
            headers += "apikey: " + juce::String(config.supabaseKey) + "\r\n";
            headers += "Authorization: Bearer " + juce::String(config.supabaseKey) + "\r\n";
        }
        options.withExtraHeaders(headers);
        options.withConnectionTimeoutMs(config.requestTimeoutMs);

        url = url.withPOSTData(jsonBody);

        auto stream = url.createInputStream(options);
        if (!stream) {
            return ActivationStatus::NetworkError;
        }

        auto response = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(response);

        if (json.isVoid() || !json.getDynamicObject()) {
            return ActivationStatus::ServerError;
        }

        auto* obj = json.getDynamicObject();

        // Check for specific errors
        if (obj->hasProperty("error")) {
            auto error = obj->getProperty("error").toString().toStdString();
            if (error.find("revoked") != std::string::npos) {
                std::lock_guard<std::mutex> lock(mutex);
                activationInfo.isValid = false;
                return ActivationStatus::Revoked;
            }
            if (error.find("Invalid") != std::string::npos) {
                std::lock_guard<std::mutex> lock(mutex);
                activationInfo.isValid = false;
                return ActivationStatus::Invalid;
            }
            return ActivationStatus::ServerError;
        }

        // Check valid flag
        bool isValid = obj->getProperty("valid");
        {
            std::lock_guard<std::mutex> lock(mutex);
            activationInfo.isValid = isValid;
        }

        return isValid ? ActivationStatus::Valid : ActivationStatus::Invalid;
#else
        return ActivationStatus::NotConfigured;
#endif
    }

    void activateAsync(const std::string& code, StatusCallback callback) {
        std::thread([this, code, callback]() {
            auto status = activate(code);
            if (callback) {
                callback(status);
            }
        }).detach();
    }

    void validateAsync(StatusCallback callback) {
        std::thread([this, callback]() {
            auto status = validate();
            if (callback) {
                callback(status);
            }
        }).detach();
    }

    void loadState() {
#if BEATCONNECT_USE_JUCE
        if (statePath.empty()) {
            return;
        }

        juce::File file(statePath);

        if (file.existsAsFile()) {
            activated = true;
            activationInfo.isValid = true;

            // Try to load full state from JSON
            auto content = file.loadFileAsString();
            auto json = juce::JSON::parse(content);
            if (auto* obj = json.getDynamicObject()) {
                if (obj->hasProperty("activation_code")) {
                    activationInfo.activationCode = obj->getProperty("activation_code").toString().toStdString();
                }
                if (obj->hasProperty("machine_id")) {
                    activationInfo.machineId = obj->getProperty("machine_id").toString().toStdString();
                }
                if (obj->hasProperty("activated_at")) {
                    activationInfo.activatedAt = obj->getProperty("activated_at").toString().toStdString();
                }
                if (obj->hasProperty("current_activations")) {
                    activationInfo.currentActivations = static_cast<int>(obj->getProperty("current_activations"));
                }
                if (obj->hasProperty("max_activations")) {
                    activationInfo.maxActivations = static_cast<int>(obj->getProperty("max_activations"));
                }
            }
        }
#endif
    }

    void saveState() {
#if BEATCONNECT_USE_JUCE
        juce::File file(statePath);
        file.getParentDirectory().createDirectory();

        juce::DynamicObject::Ptr obj = new juce::DynamicObject();

        {
            std::lock_guard<std::mutex> lock(mutex);
            obj->setProperty("activation_code",
                juce::String(activationInfo.activationCode));
            obj->setProperty("machine_id",
                juce::String(activationInfo.machineId));
            obj->setProperty("activated_at",
                juce::String(activationInfo.activatedAt));
            obj->setProperty("is_valid", activationInfo.isValid);
            obj->setProperty("current_activations",
                activationInfo.currentActivations);
            obj->setProperty("max_activations",
                activationInfo.maxActivations);
        }

        auto json = juce::JSON::toString(juce::var(obj.get()));
        file.replaceWithText(json);
#endif
    }

    void clearState() {
#if BEATCONNECT_USE_JUCE
        juce::File file(statePath);
        if (file.existsAsFile()) {
            file.deleteFile();
        }
#endif
    }

    std::string getMachineId() const {
        return MachineId::generate();
    }

private:
    mutable std::mutex mutex;
    ActivationConfig config;
    std::string statePath;
    std::string debugLogPath;
    bool configured = false;
    bool activated = false;
    bool debugEnabled = false;
    ActivationInfo activationInfo;
    Activation::DebugCallback debugCallback;
};

// ==============================================================================
// Activation Public Interface
// ==============================================================================

Activation::Activation() : pImpl(std::make_unique<Impl>()) {}
Activation::~Activation() = default;

// Move operations
Activation::Activation(Activation&&) noexcept = default;
Activation& Activation::operator=(Activation&&) noexcept = default;

void Activation::configure(const ActivationConfig& config) {
    pImpl->configure(config);
}

bool Activation::isConfigured() const {
    return pImpl->isConfigured();
}

bool Activation::isActivated() const {
    return pImpl->isActivated();
}

std::optional<ActivationInfo> Activation::getActivationInfo() const {
    return pImpl->getActivationInfo();
}

ActivationStatus Activation::activate(const std::string& code) {
    return pImpl->activate(code);
}

ActivationStatus Activation::deactivate() {
    return pImpl->deactivate();
}

ActivationStatus Activation::validate() {
    return pImpl->validate();
}

void Activation::activateAsync(const std::string& code, StatusCallback callback) {
    pImpl->activateAsync(code, callback);
}

void Activation::validateAsync(StatusCallback callback) {
    pImpl->validateAsync(callback);
}

void Activation::loadState() {
    pImpl->loadState();
}

void Activation::saveState() {
    pImpl->saveState();
}

void Activation::clearState() {
    pImpl->clearState();
}

std::string Activation::getMachineId() const {
    return pImpl->getMachineId();
}

void Activation::setDebugCallback(DebugCallback callback) {
    pImpl->setDebugCallback(callback);
}

void Activation::setDebugEnabled(bool enabled) {
    pImpl->setDebugEnabled(enabled);
}

bool Activation::isDebugEnabled() const {
    return pImpl->isDebugEnabled();
}

std::string Activation::getDebugLogPath() const {
    return pImpl->getDebugLogPath();
}

} // namespace beatconnect
