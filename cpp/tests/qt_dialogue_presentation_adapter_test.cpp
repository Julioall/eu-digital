#include "shell/qt_dialogue_presentation_adapter.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <cassert>

using namespace eu_digital;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QObject context;
    QString received;
    QtDialoguePresentationAdapter adapter(
        &context, [&](const QString& text) { received = text; });

    contracts::CognitiveOutputRequestV1 request;
    request.request_id = "request-1";
    request.correlation_id = "correlation-1";
    request.input_event_id = "event-1";
    request.intent = contracts::CognitiveOutputIntentV1::requested_response;
    request.occurred_at = "2026-08-04T12:00:00Z";
    request.critical = true;
    request.reason = "fixture";
    request.evidence_refs = {"event-1"};
    const auto output = contracts::ValidatedDialogueOutputV1::fallback(
        request, "renderer-1", "renderer_unavailable");

    const auto result = adapter.present(output);
    assert(result.valid() && result.success && *result.value);
    QTimer::singleShot(0, &app, &QCoreApplication::quit);
    app.exec();
    assert(received == QString::fromUtf8(output.rendered_text));

    const auto silence = contracts::ValidatedDialogueOutputV1::silence(
        request, "renderer-1", "silence");
    const auto rejected = adapter.present(silence);
    assert(rejected.valid() && !rejected.success);
    assert(rejected.error->code == "output_not_presentable");
}
