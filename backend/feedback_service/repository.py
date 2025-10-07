"""Repository coordinating feedback persistence, RLHF экспорт и адаптацию θ."""

from __future__ import annotations

import logging
from typing import Annotated

from fastapi import Depends

from .database import FeedbackStorage, FeedbackStorageError, get_feedback_storage
from .rlhf_dataset import RLHFDatasetWriter, get_dataset_writer
from .schemas import FeedbackPayload, FeedbackRecord
from .theta import ThetaUpdater, get_theta_updater

logger = logging.getLogger(__name__)


class FeedbackRepository:
    """High level orchestrator combining storage, RLHF export и θ-адаптацию."""

    def __init__(
        self,
        storage: FeedbackStorage,
        dataset_writer: RLHFDatasetWriter,
        theta_updater: ThetaUpdater,
    ) -> None:
        self._storage = storage
        self._dataset_writer = dataset_writer
        self._theta_updater = theta_updater

    async def create_feedback(self, payload: FeedbackPayload) -> FeedbackRecord:
        """Persist the payload and mirror it into the RLHF dataset."""

        record = await self._storage.save_feedback(payload)
        await self._dataset_writer.append(record)

        try:
            await self._theta_updater.update(record)
        except Exception as error:  # pragma: no cover - защитный путь
            logger.exception("Не удалось обновить θ на основе отзыва: %s", error)

        return record


async def get_repository(
    storage: Annotated[FeedbackStorage, Depends(get_feedback_storage)],
    dataset_writer: Annotated[RLHFDatasetWriter, Depends(get_dataset_writer)],
    theta_updater: Annotated[ThetaUpdater, Depends(get_theta_updater)],
) -> FeedbackRepository:
    """FastAPI dependency constructing a feedback repository instance."""

    return FeedbackRepository(storage, dataset_writer, theta_updater)


__all__ = [
    "FeedbackRepository",
    "FeedbackStorageError",
    "ThetaUpdater",
    "get_repository",
]
