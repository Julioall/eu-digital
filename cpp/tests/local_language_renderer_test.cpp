#include "core/adapters/local_language_renderer.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

using namespace eu_digital;

void test_success() {
    LocalLanguageRenderer renderer([](const std::string&) {
        return "Olá, tudo bem?";
    });

    CognitiveOutputRequest req{"requested_response", "{}", {}, "Say hi"};
    auto out = renderer.render(req);
    
    assert(out.status == "rendered");
    assert(out.rendered_text == "Olá, tudo bem?");
    std::cout << "test_success passed\n";
}

void test_timeout() {
    LocalLanguageRenderer renderer([](const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return "Demorou mas chegou";
    }, 10); // timeout 10ms

    CognitiveOutputRequest req{"requested_response", "{}", {}, "Say hi"};
    auto out = renderer.render(req);
    
    assert(out.status == "fallback_used");
    std::cout << "test_timeout passed\n";
}

void test_malformed() {
    LocalLanguageRenderer renderer([](const std::string&) {
        return "malformed"; // Simulating malformed JSON
    });

    CognitiveOutputRequest req{"requested_response", "{}", {}, "Say hi"};
    auto out = renderer.render(req);
    
    assert(out.status == "fallback_used");
    std::cout << "test_malformed passed\n";
}

void test_silence() {
    LocalLanguageRenderer renderer([](const std::string&) {
        return "Should not be called";
    });

    CognitiveOutputRequest req{"silence", "{}", {}, ""};
    auto out = renderer.render(req);
    
    assert(out.status == "silence");
    assert(out.rendered_text == "");
    std::cout << "test_silence passed\n";
}

int main() {
    test_success();
    test_timeout();
    test_malformed();
    test_silence();
    return 0;
}
