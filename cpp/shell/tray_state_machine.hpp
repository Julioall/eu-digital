#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <iostream>

namespace eu_digital {

/// SPEC-052 §States — Operational presence of the cognitive runtime.
enum class PresenceState {
    active,       ///< Runtime available and idle
    processing,   ///< Request being processed
    asking,       ///< Question or confirmation awaiting user
    paused,       ///< Observation suspended
    offline,      ///< Runtime unavailable
    degraded,     ///< Runtime active with reduced capacity
};

/// SPEC-052 §Modos da superfície — Which UI surface is currently open.
enum class SurfaceMode {
    tray_only,    ///< Only tray icon visible (default)
    compact,      ///< Compact quick-ask widget
    expanded,     ///< Full dialogue widget
    quick_panel,  ///< Operational summary panel
    settings,     ///< Settings window
    confirmation, ///< Supervised action confirmation
    diagnostics,  ///< Explicit diagnostics view
};

inline QString presenceStateName(PresenceState s) {
    switch (s) {
        case PresenceState::active:     return "ativo";
        case PresenceState::processing: return "processando";
        case PresenceState::asking:     return "perguntando";
        case PresenceState::paused:     return "pausado";
        case PresenceState::offline:    return "offline";
        case PresenceState::degraded:   return "degradado";
    }
    return "desconhecido";
}

inline QString surfaceModeName(SurfaceMode m) {
    switch (m) {
        case SurfaceMode::tray_only:    return "tray_only";
        case SurfaceMode::compact:      return "compact";
        case SurfaceMode::expanded:     return "expanded";
        case SurfaceMode::quick_panel:  return "quick_panel";
        case SurfaceMode::settings:     return "settings";
        case SurfaceMode::confirmation: return "confirmation";
        case SurfaceMode::diagnostics:  return "diagnostics";
    }
    return "unknown";
}

/// SPEC-052 §Estados e transições
/// Validates and manages presence and surface transitions.
/// Invalid transitions are rejected and logged with structured error.
class TrayStateMachine : public QObject {
    Q_OBJECT
public:
    explicit TrayStateMachine(QObject* parent = nullptr)
        : QObject(parent)
        , presence_(PresenceState::offline)
        , surface_(SurfaceMode::tray_only)
    {}

    PresenceState presence() const { return presence_; }
    SurfaceMode   surface()  const { return surface_; }

    /// Request a presence transition. Returns false if invalid.
    bool requestPresence(PresenceState next, const QString& reason = {}) {
        if (!isPresenceTransitionAllowed(presence_, next)) {
            std::cerr << "[TrayStateMachine] REJECTED presence transition: "
                      << presenceStateName(presence_).toStdString()
                      << " -> " << presenceStateName(next).toStdString()
                      << "\n";
            return false;
        }
        presence_ = next;
        last_transition_reason_ = reason;
        emit presenceChanged(presence_);
        return true;
    }

    /// Request a surface transition. Returns false if invalid.
    bool requestSurface(SurfaceMode next) {
        if (!isSurfaceTransitionAllowed(surface_, next)) {
            std::cerr << "[TrayStateMachine] REJECTED surface transition: "
                      << surfaceModeName(surface_).toStdString()
                      << " -> " << surfaceModeName(next).toStdString()
                      << "\n";
            return false;
        }
        surface_ = next;
        emit surfaceChanged(surface_);
        return true;
    }

    QString lastTransitionReason() const { return last_transition_reason_; }

signals:
    void presenceChanged(eu_digital::PresenceState state);
    void surfaceChanged(eu_digital::SurfaceMode mode);

private:
    /// SPEC-052 §Presença transitions
    static bool isPresenceTransitionAllowed(PresenceState from, PresenceState to) {
        using P = PresenceState;
        if (from == to) return true;
        // Any state -> offline is always allowed
        if (to == P::offline) return true;
        switch (from) {
            case P::active:
                return to == P::processing || to == P::paused || to == P::degraded;
            case P::processing:
                return to == P::active || to == P::asking || to == P::paused || to == P::degraded;
            case P::asking:
                return to == P::active || to == P::paused;
            case P::paused:
                return to == P::active || to == P::degraded;
            case P::offline:
                return to == P::active || to == P::paused || to == P::degraded;
            case P::degraded:
                return to == P::active || to == P::paused || to == P::offline;
        }
        return false;
    }

    /// SPEC-052 §Superfície transitions
    static bool isSurfaceTransitionAllowed(SurfaceMode from, SurfaceMode to) {
        using S = SurfaceMode;
        if (from == to) return true;
        // Any surface -> tray_only is always allowed (close/dismiss)
        if (to == S::tray_only) return true;
        switch (from) {
            case S::tray_only:
                return to == S::compact;
            case S::compact:
                return to == S::expanded || to == S::quick_panel || to == S::settings;
            case S::expanded:
                return to == S::compact;
            case S::quick_panel:
                return to == S::compact;
            case S::settings:
                return true; // settings -> any (back navigation)
            case S::confirmation:
                return to == S::tray_only || to == S::expanded;
            case S::diagnostics:
                return to == S::tray_only || to == S::settings;
        }
        return false;
    }

    PresenceState presence_;
    SurfaceMode   surface_;
    QString       last_transition_reason_;
};

} // namespace eu_digital
