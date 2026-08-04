from __future__ import annotations

import copy
import hashlib
import json
import unittest
from pathlib import Path

from eu_digital_lab.schema_validation import (
    SchemaValidationError,
    validate_shared_schema,
)

ROOT = Path(__file__).resolve().parents[2]


class OllamaBackendContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.config = json.loads(
            (ROOT / "config" / "ollama_backend.json").read_text(encoding="utf-8")
        )
        self.binding = json.loads(
            (ROOT / "models" / "manifests" / "qwen3-vl-2b-ollama.json").read_text(
                encoding="utf-8"
            )
        )

    def test_repository_configuration_and_binding_satisfy_shared_schemas(self) -> None:
        validate_shared_schema(self.config, "ollama_backend_config.schema.json")
        validate_shared_schema(self.binding, "ollama_model_binding.schema.json")

        artifact = self.binding["artifact"]
        signed = ":".join(
            (
                "eu-digital-model-signature-v1",
                artifact["signing_key_id"],
                artifact["model_id"],
                artifact["sha256"],
                artifact["runtime_artifact_id"],
                artifact["payload_artifact_id"],
            )
        )
        self.assertEqual(
            artifact["signature"],
            hashlib.sha256(signed.encode("utf-8")).hexdigest(),
        )

    def test_remote_endpoint_and_unknown_configuration_are_rejected(self) -> None:
        remote = copy.deepcopy(self.config)
        remote["host"] = "localhost"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(remote, "ollama_backend_config.schema.json")

        extra = copy.deepcopy(self.config)
        extra["proxy"] = "http://proxy.invalid"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(extra, "ollama_backend_config.schema.json")

    def test_cloud_or_unbound_model_metadata_is_rejected(self) -> None:
        cloud = copy.deepcopy(self.binding)
        cloud["artifact"]["backend_compatibility"] = "ollama.cloud"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(cloud, "ollama_model_binding.schema.json")

        invalid_digest = copy.deepcopy(self.binding)
        invalid_digest["ollama_digest"] = "not-a-digest"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(invalid_digest, "ollama_model_binding.schema.json")


if __name__ == "__main__":
    unittest.main()
