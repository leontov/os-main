import { FormEvent, useCallback, useState } from "react";
import { twMerge } from "tailwind-merge";
import { submitFeedback } from "../core/api";
import type { ChatMessage } from "../types/chat";
import type { FeedbackRating } from "../types/feedback";

interface FeedbackFormProps {
  conversationId: string;
  message: ChatMessage;
  latestUserMessage?: ChatMessage;
}

const ratingLabels: Record<FeedbackRating, string> = {
  useful: "Полезно",
  not_useful: "Не полезно",
};

const FeedbackForm = ({ conversationId, message, latestUserMessage }: FeedbackFormProps) => {
  const [rating, setRating] = useState<FeedbackRating | null>(null);
  const [comment, setComment] = useState("");
  const [status, setStatus] = useState<"idle" | "submitting" | "success">("idle");
  const [error, setError] = useState<string | null>(null);

  const isSubmitting = status === "submitting";
  const isCompleted = status === "success";

  const assistantMessage = message.content.trim();
  const userMessage = latestUserMessage?.content?.trim() || undefined;

  const handleRatingSelect = useCallback((value: FeedbackRating) => {
    setRating((current) => (current === value ? null : value));
    setError(null);
  }, []);

  const handleSubmit = useCallback(
    async (event: FormEvent<HTMLFormElement>) => {
      event.preventDefault();

      if (!rating) {
        setError("Пожалуйста, выберите оценку ответа.");
        return;
      }

      setStatus("submitting");
      setError(null);

      try {
        await submitFeedback({
          conversationId,
          messageId: message.id,
          assistantMessage,
          userMessage,
          rating,
          comment: comment.trim() || undefined,
          mode: message.mode,
        });
        setStatus("success");
      } catch (feedbackError) {
        setStatus("idle");
        setError(
          feedbackError instanceof Error
            ? feedbackError.message
            : "Не удалось отправить отзыв. Попробуйте ещё раз.",
        );
      }
    },
    [assistantMessage, comment, conversationId, message.id, message.mode, rating, userMessage],
  );

  if (!assistantMessage.length) {
    return null;
  }

  return (
    <form className="mt-4 space-y-3 rounded-xl bg-background-light/60 p-4" onSubmit={handleSubmit}>
      <p className="text-sm font-medium text-text-dark">Оцените ответ</p>
      <div className="flex flex-wrap gap-3">
        {(Object.keys(ratingLabels) as FeedbackRating[]).map((value) => {
          const isSelected = rating === value;
          return (
            <button
              key={value}
              type="button"
              className={twMerge(
                "rounded-full border border-primary px-4 py-2 text-sm font-medium transition-colors",
                isSelected ? "bg-primary text-white" : "bg-white text-primary hover:bg-primary/10",
                (isSubmitting || isCompleted) && "cursor-not-allowed opacity-70",
              )}
              onClick={() => handleRatingSelect(value)}
              disabled={isSubmitting || isCompleted}
            >
              {ratingLabels[value]}
            </button>
          );
        })}
      </div>

      <label className="block text-sm text-text-light">
        <span className="mb-1 block">Комментарий (необязательно)</span>
        <textarea
          className="h-24 w-full resize-none rounded-xl border border-border-light bg-white/90 p-3 text-sm text-text-dark focus:border-primary focus:outline-none"
          placeholder="Поделитесь деталями, чтобы помочь нам улучшить ответы"
          value={comment}
          onChange={(event) => setComment(event.target.value)}
          disabled={isSubmitting || isCompleted}
          maxLength={1000}
        />
      </label>

      {error && <p className="text-sm text-accent-coral">{error}</p>}

      <div className="flex items-center gap-3">
        <button
          type="submit"
          className={twMerge(
            "rounded-full bg-primary px-5 py-2 text-sm font-semibold text-white transition-opacity",
            (isSubmitting || isCompleted || !rating) && "opacity-70",
          )}
          disabled={isSubmitting || isCompleted || !rating}
        >
          {isSubmitting ? "Отправка..." : isCompleted ? "Отправлено" : "Отправить"}
        </button>
        {isCompleted && <span className="text-sm text-text-light">Спасибо за отзыв!</span>}
      </div>
    </form>
  );
};

export default FeedbackForm;
