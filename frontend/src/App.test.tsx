import { act, render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { beforeEach, describe, expect, it, vi, type MockInstance } from "vitest";
import App from "./App";

type AskFunction = (prompt: string, mode?: string) => Promise<string>;
type ResetFunction = () => Promise<void>;
type SearchFunction = (
  query: string,
  options?: { topK?: number; signal?: AbortSignal }
) => Promise<Array<{ id: string; title: string; content: string; score: number }>>;

const { askMock, resetMock, searchMock } = vi.hoisted(() => ({
  askMock: vi.fn<Parameters<AskFunction>, ReturnType<AskFunction>>(),
  resetMock: vi.fn<Parameters<ResetFunction>, ReturnType<ResetFunction>>(),
  searchMock: vi.fn<Parameters<SearchFunction>, ReturnType<SearchFunction>>(),
}));

vi.mock("./core/kolibri-bridge", () => ({
  default: {
    ready: Promise.resolve(),
    ask: askMock,
    reset: resetMock,
  },
}));

vi.mock("./core/knowledge", () => ({
  searchKnowledge: searchMock,
}));

describe("App contextual retrieval", () => {
  let consoleErrorSpy: MockInstance<[message?: unknown, ...optionalParams: unknown[]], void>;

  beforeEach(() => {
    askMock.mockReset();
    resetMock.mockReset();
    searchMock.mockReset();
    resetMock.mockResolvedValue(undefined);
    consoleErrorSpy = vi.spyOn(console, "error");
    consoleErrorSpy.mockImplementation(() => {});
  });

  afterEach(() => {
    consoleErrorSpy.mockRestore();
  });

  it("prepends retrieved context to the prompt and surfaces it in the UI", async () => {
    searchMock.mockResolvedValue([
      { id: "1", title: "Документ", content: "Описание Kolibri", score: 0.92 },
    ]);
    askMock.mockResolvedValue("Ответ с контекстом");

    await act(async () => {
      render(<App />);
    });

    const textarea = screen.getByPlaceholderText("Сообщение для Колибри");
    const sendButton = screen.getByRole("button", { name: "Отправить" });

    await userEvent.type(textarea, "Что такое Kolibri?");
    await waitFor(() => expect(sendButton).not.toBeDisabled());
    await userEvent.click(sendButton);

    await waitFor(() => expect(askMock).toHaveBeenCalled());

    const [prompt, mode] = askMock.mock.calls[0] ?? [];
    expect(prompt).toContain("Контекст:");
    expect(prompt).toContain("Описание Kolibri");
    expect(mode).toBe("Быстрый ответ");

    await waitFor(() => expect(screen.getByText("Ответ с контекстом")).toBeInTheDocument());

    const toggle = screen.getByRole("button", { name: "Показать контекст (1)" });
    await userEvent.click(toggle);
    expect(screen.getByText("Документ")).toBeInTheDocument();
    expect(screen.getByText("Описание Kolibri")).toBeInTheDocument();
  });

  it("falls back gracefully when contextual search fails", async () => {
    searchMock.mockRejectedValue(new Error("модуль памяти недоступен"));
    askMock.mockResolvedValue("Ответ без контекста");

    await act(async () => {
      render(<App />);
    });

    const textarea = screen.getByPlaceholderText("Сообщение для Колибри");
    const sendButton = screen.getByRole("button", { name: "Отправить" });

    await userEvent.type(textarea, "Где хранится знание?");
    await waitFor(() => expect(sendButton).not.toBeDisabled());
    await userEvent.click(sendButton);

    await waitFor(() => expect(askMock).toHaveBeenCalled());

    const [prompt] = askMock.mock.calls[0] ?? [];
    expect(prompt).toBe("Где хранится знание?");

    await waitFor(() => expect(screen.getByText("Ответ без контекста")).toBeInTheDocument());

    expect(screen.getByText(/Контекст недоступен:/)).toHaveTextContent("модуль памяти недоступен");
    expect(screen.queryByRole("button", { name: /Контекст/ })).not.toBeInTheDocument();
  });
});
