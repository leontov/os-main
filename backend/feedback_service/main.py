"""FastAPI application exposing the feedback submission endpoint."""

from __future__ import annotations

import logging
from contextlib import asynccontextmanager

from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.middleware.cors import CORSMiddleware

from .database import FeedbackStorageError, shutdown_feedback_storage
from .repository import FeedbackRepository, get_repository
from .schemas import FeedbackPayload, FeedbackResponse

logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Clean up shared resources on shutdown."""

    yield
    await shutdown_feedback_storage()


app = FastAPI(title="Kolibri Feedback API", version="1.0.0", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["POST", "OPTIONS"],
    allow_headers=["*"]
)


@app.post("/api/feedback", response_model=FeedbackResponse, status_code=status.HTTP_201_CREATED)
async def submit_feedback(
    payload: FeedbackPayload,
    repository: FeedbackRepository = Depends(get_repository),
):
    """Persist feedback and append it to the RLHF dataset."""

    try:
        await repository.create_feedback(payload)
    except FeedbackStorageError as error:
        logger.exception("Feedback persistence failed: %s", error)
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=str(error)) from error
    except Exception as error:  # pragma: no cover - defensive logging
        logger.exception("Unexpected error while handling feedback: %s", error)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Не удалось сохранить отзыв.",
        ) from error

    return FeedbackResponse()


__all__ = ["app"]
