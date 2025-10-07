import type { FeedbackRequest } from "../types/feedback";

export const API_BASE_URL = (import.meta.env.VITE_API_BASE_URL ?? "").replace(/\/$/, "");
const FEEDBACK_ENDPOINT = "/api/feedback";

export const buildUrl = (endpoint: string) => `${API_BASE_URL}${endpoint}`;

export const submitFeedback = async (payload: FeedbackRequest): Promise<void> => {
  const response = await fetch(buildUrl(FEEDBACK_ENDPOINT), {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(payload),
  });

  if (response.ok) {
    return;
  }

  let detail = "Не удалось сохранить отзыв.";

  try {
    const data = (await response.json()) as { detail?: string } | undefined;
    if (data?.detail) {
      detail = data.detail;
    }
  } catch (error) {
    // Игнорируем ошибки парсинга и используем сообщение по умолчанию.
  }

  throw new Error(detail);
};
