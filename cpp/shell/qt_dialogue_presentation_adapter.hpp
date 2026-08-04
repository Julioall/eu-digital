#pragma once

#include "core/ports/ipresentation_port.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <stdexcept>
#include <utility>

namespace eu_digital {

class QtDialoguePresentationAdapter final : public IPresentationPort {
public:
    using PresentCallback = std::function<void(const QString&)>;

    QtDialoguePresentationAdapter(QObject* ui_context,
                                  PresentCallback callback)
        : ui_context_(ui_context), callback_(std::move(callback)) {
        if (!ui_context || !callback_) {
            throw std::invalid_argument(
                "Qt presentation requires a UI context and callback");
        }
    }

    contracts::PortResult<bool> present(
        const contracts::ValidatedDialogueOutputV1& output) override {
        if (!output.presentable()) {
            return contracts::PortResult<bool>::failed(
                kPresentationOperation, "output_not_presentable",
                "only validated rendered or fallback output may reach Qt");
        }
        if (ui_context_.isNull()) {
            return contracts::PortResult<bool>::failed(
                kPresentationOperation, "ui_context_unavailable",
                "the Qt UI context was destroyed");
        }
        const QPointer<QObject> context = ui_context_;
        const auto callback = callback_;
        const auto text = QString::fromUtf8(
            output.rendered_text.data(),
            static_cast<qsizetype>(output.rendered_text.size()));
        const bool queued = QMetaObject::invokeMethod(
            ui_context_.data(),
            [context, callback, text] {
                if (!context.isNull()) callback(text);
            },
            Qt::QueuedConnection);
        if (!queued) {
            return contracts::PortResult<bool>::failed(
                kPresentationOperation, "qt_enqueue_failed",
                "Qt rejected the queued presentation callback", true);
        }
        return contracts::PortResult<bool>::ok(true);
    }

private:
    QPointer<QObject> ui_context_;
    PresentCallback callback_;
};

}  // namespace eu_digital
