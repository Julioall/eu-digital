"""Deterministic research-lab primitives for the Eu Digital sandbox."""

from .capabilities import (
    CapabilityDescriptor,
    CapabilityRegistry,
    CapabilityRuntime,
    CapabilityState,
    ModuleLifecycleManager,
    PluginDiscovery,
)
from .episode_segmentation import (
    BoundaryDecision,
    SegmentationError,
    SegmentationResult,
    SegmentConfig,
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
from .evaluation import (
    DatasetRepository,
    ExperimentConfig,
    ExperimentReport,
    ExperimentRunner,
)
from .global_workspace import (
    GlobalWorkspace,
    WorkspaceCandidate,
    WorkspaceConfig,
    WorkspaceError,
    WorkspaceItem,
    WorkspaceSnapshot,
    evaluate_selection,
)
from .promotion import PromotionManifest, PromotionPipeline, PromotionRegistry
from .sandbox import SyntheticSession, generate_session, split_sessions
from .validation import ValidationGateRunner, ValidationProtocol

__all__ = [
    "BoundaryDecision",
    "CapabilityDescriptor",
    "CapabilityRegistry",
    "CapabilityRuntime",
    "CapabilityState",
    "DatasetRepository",
    "EpisodicMemory",
    "EpisodicMemoryError",
    "ExperimentConfig",
    "ExperimentReport",
    "ExperimentRunner",
    "GlobalWorkspace",
    "MemoryQuery",
    "ModuleLifecycleManager",
    "PluginDiscovery",
    "PromotionManifest",
    "PromotionPipeline",
    "PromotionRegistry",
    "RetrievalResult",
    "SegmentConfig",
    "SegmentationError",
    "SegmentationResult",
    "SegmentedEpisode",
    "StoreResult",
    "SyntheticSession",
    "ValidationGateRunner",
    "ValidationProtocol",
    "WorkspaceCandidate",
    "WorkspaceConfig",
    "WorkspaceError",
    "WorkspaceItem",
    "WorkspaceSnapshot",
    "boundary_metrics",
    "evaluate_baseline",
    "evaluate_selection",
    "generate_session",
    "segment_events",
    "split_sessions",
]
