#pragma once

#include "core/adapters/ollama_model_backend.hpp"
#include "core/local_model_gateway.hpp"

#include <QObject>
#include <QString>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <string>
#include <atomic>
#include <chrono>
#include <vector>
#include <sstream>

namespace eu_digital {

/// OllamaDialogueService (SPEC-051)
/// Non-blocking bridge between the Tray UI and the OllamaModelBackend.
/// Calls Ollama in a background thread via QtConcurrent so the UI stays responsive.
class OllamaDialogueService : public QObject {
    Q_OBJECT
public:
    explicit OllamaDialogueService(const std::string& model_id = "qwen3-vl:2b",
                                   QObject* parent = nullptr)
        : QObject(parent), model_id_(model_id)
    {
        LocalModelArtifact artifact;
        artifact.model_id = model_id_;
        try { backend_.load(artifact); } catch (...) {}
    }

    void sendAsync(const QString& user_text) {
        if (pending_.load()) {
            emit errorOccurred("Aguardando resposta anterior...");
            return;
        }
        pending_.store(true);

        std::string raw_text = user_text.toStdString();
        std::string model  = model_id_;
        
        // Build prompt with ChatML format to pass history
        std::ostringstream oss;
        oss << "<|im_start|>system\nVocê é o Eu Digital, um assistente AI focado em monitorar e ajudar o usuário localmente. Seja direto e prestativo.<|im_end|>\n";
        
        for (const auto& pair : history_) {
            oss << "<|im_start|>user\n" << pair.first << "<|im_end|>\n";
            oss << "<|im_start|>assistant\n" << pair.second << "<|im_end|>\n";
        }
        oss << "<|im_start|>user\n" << raw_text << "<|im_end|>\n";
        oss << "<|im_start|>assistant\n";
        
        std::string prompt = oss.str();

        auto* watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this,
                [this, watcher, raw_text]() {
                    pending_.store(false);
                    try {
                        QString result = watcher->result();
                        // Append to history
                        history_.push_back({raw_text, result.toStdString()});
                        if (history_.size() > 10) {
                            history_.erase(history_.begin());
                        }
                        emit responseReady(result);
                    } catch (const std::exception& e) {
                        emit errorOccurred(QString("Ollama erro: %1").arg(e.what()));
                    } catch (...) {
                        emit errorOccurred("Erro desconhecido ao chamar Ollama.");
                    }
                    watcher->deleteLater();
                });

        OllamaModelBackend* bp = &backend_;
        QFuture<QString> future = QtConcurrent::run([bp, prompt, model]() -> QString {
            LocalModelRequest req;
            req.request_id      = "ui-req-" + std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
            req.backend_id      = "ollama";
            req.model_id        = model;
            req.rendered_prompt = prompt;
            req.timeout_seconds = 120.0;
            req.template_value.template_id = "raw";
            req.template_value.version     = "1.0";

            LocalModelRawOutput out = bp->invoke(req);
            auto it = out.fields.find("text");
            if (it != out.fields.end() && !it->second.empty())
                return QString::fromStdString(it->second);
            return QString("(resposta vazia)");
        });
        watcher->setFuture(future);
    }

    void setModel(const std::string& model_id) {
        model_id_ = model_id;
        LocalModelArtifact artifact;
        artifact.model_id = model_id_;
        try { backend_.load(artifact); } catch (...) {}
    }

signals:
    void responseReady(const QString& text);
    void errorOccurred(const QString& message);

private:
    OllamaModelBackend backend_;
    std::string        model_id_;
    std::atomic<bool>  pending_{false};
    std::vector<std::pair<std::string, std::string>> history_;
};

} // namespace eu_digital
