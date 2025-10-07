import { useEffect, useMemo, useRef } from "react";
import type { ChatMessage } from "../types/chat";
import ChatMessageView from "./ChatMessage";

interface ChatViewProps {
  messages: ChatMessage[];
  isLoading: boolean;
  conversationId: string;
}

const ChatView = ({ messages, isLoading, conversationId }: ChatViewProps) => {
  const containerRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    const container = containerRef.current;

    if (!container) {
      return;
    }

    container.scrollTop = container.scrollHeight;
  }, [messages, isLoading]);

  const renderedMessages = useMemo(() => {
    let lastUserMessage: ChatMessage | undefined;
    return messages.map((message) => {
      const contextUserMessage = message.role === "assistant" ? lastUserMessage : undefined;
      if (message.role === "user") {
        lastUserMessage = message;
      }

      return (
        <ChatMessageView
          key={message.id}
          message={message}
          conversationId={conversationId}
          latestUserMessage={contextUserMessage}
        />
      );
    });
  }, [conversationId, messages]);

  return (
    <section className="flex h-full flex-col rounded-3xl bg-white/70 p-8 shadow-card">
      <div className="flex-1 space-y-6 overflow-y-auto pr-2" ref={containerRef}>
        {renderedMessages}
        {isLoading && (
          <div className="flex items-center gap-3 text-sm text-text-light">
            <span className="h-2 w-2 animate-pulse rounded-full bg-primary" />
            Колибри формирует ответ...
          </div>
        )}
        {!messages.length && !isLoading && (
          <div className="rounded-2xl bg-background-light/60 p-6 text-sm text-text-light">
            Отправь сообщение, чтобы начать диалог с Колибри.
          </div>
        )}
      </div>
    </section>
  );
};

export default ChatView;
