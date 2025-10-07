import { useCallback, useEffect, useMemo, useState } from "react";
import Layout from "./components/Layout";
import Sidebar from "./components/Sidebar";
import WelcomeScreen from "./components/WelcomeScreen";
import ChatInput from "./components/ChatInput";
import ChatView from "./components/ChatView";
import type { ChatMessage } from "./types/chat";
import kolibriBridge from "./core/kolibri-bridge";
import { searchKnowledge } from "./core/knowledge";
import type { KnowledgeSnippet } from "./types/knowledge";

const formatPromptWithContext = (question: string, context: KnowledgeSnippet[]): string => {
  if (!context.length) {
    return question;
  }

  const contextBlocks = context.map((snippet, index) => {
    const title = snippet.title ? ` (${snippet.title})` : "";
    return [`Источник ${index + 1}${title}:`, snippet.content].join("\n");
  });

  return [`Контекст:`, ...contextBlocks, "", `Вопрос пользователя: ${question}`].join("\n");
};

const App = () => {
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [draft, setDraft] = useState("");
  const [mode, setMode] = useState("Быстрый ответ");
  const [isProcessing, setIsProcessing] = useState(false);
  const [bridgeReady, setBridgeReady] = useState(false);
  const [conversationId, setConversationId] = useState(() => crypto.randomUUID());

  useEffect(() => {
    let cancelled = false;
    kolibriBridge.ready
      .then(() => {
        if (!cancelled) {
          setBridgeReady(true);
        }
      })
      .catch((error) => {
        if (cancelled) {
          return;
        }
        const assistantMessage: ChatMessage = {
          id: crypto.randomUUID(),
          role: "assistant",
          content:
            error instanceof Error
              ? `Не удалось инициализировать KolibriScript: ${error.message}`
              : "Не удалось инициализировать KolibriScript.",
          timestamp: new Date().toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" }),
        };
        setMessages((prev) => [...prev, assistantMessage]);
      });

    return () => {
      cancelled = true;
    };
  }, []);

  const handleSuggestionSelect = useCallback((prompt: string) => {
    setDraft(prompt);
  }, []);

  const resetConversation = useCallback(() => {
    if (!bridgeReady) {
      setMessages([]);
      setDraft("");
      setConversationId(crypto.randomUUID());
      setIsProcessing(false);
      return;
    }

    void (async () => {
      try {
        await kolibriBridge.reset();
        setMessages([]);
        setDraft("");
        setConversationId(crypto.randomUUID());
      } catch (error) {
        const assistantMessage: ChatMessage = {
          id: crypto.randomUUID(),
          role: "assistant",
          content:
            error instanceof Error
              ? `Не удалось сбросить KolibriScript: ${error.message}`
              : "Не удалось сбросить KolibriScript.",
          timestamp: new Date().toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" }),
        };
        setMessages((prev) => [...prev, assistantMessage]);
      } finally {
        setIsProcessing(false);
      }
    })();
  }, [bridgeReady]);

  const sendMessage = useCallback(async () => {
    const content = draft.trim();
    if (!content || isProcessing || !bridgeReady) {
      return;
    }

    const userMessage: ChatMessage = {
      id: crypto.randomUUID(),
      role: "user",
      content,
      timestamp: new Date().toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" }),
    };

    setMessages((prev) => [...prev, userMessage]);
    setDraft("");
    setIsProcessing(true);

    let knowledgeContext: KnowledgeSnippet[] = [];
    let knowledgeError: string | undefined;

    try {
      knowledgeContext = await searchKnowledge(content, { topK: 3 });
    } catch (error) {
      if (!(error instanceof DOMException && error.name === "AbortError")) {
        knowledgeError =
          error instanceof Error && error.message
            ? error.message
            : "Не удалось получить контекст из памяти.";
      }
    }

    const prompt = knowledgeContext.length ? formatPromptWithContext(content, knowledgeContext) : content;

    try {
      const answer = await kolibriBridge.ask(prompt, mode);
      const assistantMessage: ChatMessage = {
        id: crypto.randomUUID(),
        role: "assistant",
        content: answer,
        timestamp: new Date().toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" }),
        mode,
      };
      if (knowledgeContext.length) {
        assistantMessage.context = knowledgeContext;
      }
      if (knowledgeError) {
        assistantMessage.contextError = knowledgeError;
      }
      setMessages((prev) => [...prev, assistantMessage]);
    } catch (error) {
      const assistantMessage: ChatMessage = {
        id: crypto.randomUUID(),
        role: "assistant",
        content:
          error instanceof Error
            ? `Не удалось получить ответ: ${error.message}`
            : "Не удалось получить ответ от ядра Колибри.",
        timestamp: new Date().toLocaleTimeString("ru-RU", { hour: "2-digit", minute: "2-digit" }),
      };
      setMessages((prev) => [...prev, assistantMessage]);
    } finally {
      setIsProcessing(false);
    }
  }, [bridgeReady, draft, isProcessing, mode]);

  const content = useMemo(() => {
    if (!messages.length) {
      return <WelcomeScreen onSuggestionSelect={handleSuggestionSelect} />;
    }

    return (
      <ChatView
        messages={messages}
        isLoading={isProcessing}
        conversationId={conversationId}
      />
    );
  }, [conversationId, handleSuggestionSelect, isProcessing, messages]);

  return (
    <Layout sidebar={<Sidebar />}>
      <div className="flex-1">{content}</div>
      <ChatInput
        value={draft}
        mode={mode}
        isBusy={isProcessing || !bridgeReady}
        onChange={setDraft}
        onModeChange={setMode}
        onSubmit={sendMessage}
        onReset={resetConversation}
      />
    </Layout>
  );
};

export default App;
