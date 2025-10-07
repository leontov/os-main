
import { useState } from "react";



import type { ChatMessage as ChatMessageModel } from "../types/chat";

const formatScore = (value: number): string => {
  if (!Number.isFinite(value)) {
    return "—";
  }
  return value.toFixed(2);
};

interface ChatMessageProps {
  message: ChatMessageModel;
  conversationId: string;
  latestUserMessage?: ChatMessageModel;
}

const ChatMessage = ({ message, conversationId, latestUserMessage }: ChatMessageProps) => {
  const isUser = message.role === "user";
  const [isContextExpanded, setIsContextExpanded] = useState(false);
  const hasContext = !isUser && Boolean(message.context?.length);
  const contextCount = message.context?.length ?? 0;

  return (
    <div className="flex items-start gap-3">
      <div
        className={`flex h-9 w-9 items-center justify-center rounded-full ${
          isUser ? "bg-primary/20 text-primary" : "bg-accent-coral/10 text-accent-coral"
        }`}
      >
        {isUser ? "Я" : "К"}
      </div>
      <div className="rounded-2xl bg-white/80 p-4 shadow-card">
        <p className="whitespace-pre-line text-sm leading-relaxed text-text-dark">{message.content}</p>
        <p className="mt-2 text-xs text-text-light">{message.timestamp}</p>

        {!isUser && (hasContext || message.contextError) && (
          <div className="mt-3 space-y-3 border-t border-dashed border-text-light/40 pt-3 text-xs text-text-dark">
            {hasContext && (
              <div>
                <button
                  type="button"
                  onClick={() => setIsContextExpanded((prev) => !prev)}
                  className="rounded-lg bg-background-light/60 px-3 py-1 font-semibold text-primary transition-colors hover:bg-background-light"
                >
                  {isContextExpanded ? "Скрыть контекст" : "Показать контекст"} ({contextCount})
                </button>
                {isContextExpanded && (
                  <div className="mt-2 space-y-2">
                    {message.context?.map((snippet, index) => (
                      <article
                        key={snippet.id}
                        className="rounded-xl bg-background-light/70 p-3 shadow-inner"
                        aria-label={`Источник ${index + 1}`}
                      >
                        <div className="flex items-center justify-between text-[0.7rem] font-semibold text-text-light">
                          <span className="uppercase tracking-wide text-text-dark">Источник {index + 1}</span>
                          <span className="text-text-light">Релевантность: {formatScore(snippet.score)}</span>
                        </div>
                        <p className="mt-1 text-sm font-semibold text-text-dark">{snippet.title}</p>
                        <p className="mt-2 whitespace-pre-line text-[0.85rem] leading-relaxed text-text-dark/90">{snippet.content}</p>
                        {snippet.source && (
                          <p className="mt-2 text-[0.7rem] uppercase tracking-wide text-primary/80">{snippet.source}</p>
                        )}
                      </article>
                    ))}
                  </div>
                )}
              </div>
            )}
            {message.contextError && (
              <p className="rounded-lg bg-accent-coral/10 px-3 py-2 text-[0.75rem] text-accent-coral">
                Контекст недоступен: {message.contextError}
              </p>
            )}
          </div>

        )}
      </div>
    </div>
  );
};

export default ChatMessage;
