export type FeedbackRating = "useful" | "not_useful";

export interface FeedbackRequest {
  conversationId: string;
  messageId: string;
  rating: FeedbackRating;
  assistantMessage: string;
  userMessage?: string;
  comment?: string;
  mode?: string;
}
