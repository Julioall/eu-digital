"""Deterministic research-lab primitives for the Eu Digital sandbox."""

from .capabilities import (
    CapabilityDescriptor,
    CapabilityRegistry,
    CapabilityRuntime,
    CapabilityState,
    ModuleLifecycleManager,
    PluginDiscovery,
)
from .evaluation import (
    DatasetRepository,
    ExperimentConfig,
    ExperimentReport,
    ExperimentRunner,
)
from .episode_segmentation import (
    BoundaryDecision,
    SegmentConfig,
    SegmentationError,
    SegmentationResult,
    SegmentedEpisode,
    boundary_metrics,
    evaluate_baseline,
    segment_events,
)
from .episodic_memory import (
    EpisodicMemory,
    EpisodicMemoryError,
    MemoryQuery,
    RetrievalResult,
    StoreResult,
)
from .promotion import PromotionManifest, PromotionPipeline, PromotionRegistry
from .sandbox import SyntheticSession, generate_session, split_sessions
from .validation import ValidationGateRunner, ValidationProtocol

__all__ = [
    "CapabilityDescriptor",
    "CapabilityRegistry",
    "CapabilityRuntime",
    "CapabilityState",
    "BoundaryDecision",
    "DatasetRepository",
    "ExperimentConfig",
    "ExperimentReport",
    "ExperimentRunner",
    "EpisodicMemory",
    "EpisodicMemoryError",
    "MemoryQuery",
    "RetrievalResult",
    "SegmentConfig",
    "SegmentationError",
    "SegmentationResult",
    "SegmentedEpisode",
    "StoreResult",
    "ModuleLifecycleManager",
    "PluginDiscovery",
    "PromotionManifest",
    "PromotionPipeline",
    "PromotionRegistry",
    "SyntheticSession",
    "ValidationGateRunner",
    "ValidationProtocol",
    "boundary_metrics",
    "evaluate_baseline",
    "generate_session",
    "split_sessions",
    "segment_events",
]
