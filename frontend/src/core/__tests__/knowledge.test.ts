import { afterEach, describe, expect, it, vi } from "vitest";
import { buildSearchUrl, searchKnowledge } from "../knowledge";

afterEach(() => {
  vi.unstubAllGlobals();
  vi.resetAllMocks();
});

describe("knowledge search helpers", () => {
  it("builds search URLs with optional limit", () => {
    expect(buildSearchUrl("kolibri")).toBe("/knowledge/search?q=kolibri");
    expect(buildSearchUrl("kolibri", { topK: 5 })).toBe("/knowledge/search?q=kolibri&limit=5");
  });

  it("returns snippets when the backend responds successfully", async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({
        snippets: [
          { id: "a", title: "Doc", content: "Kolibri description", score: 0.9 },
        ],
      }),
    });
    vi.stubGlobal("fetch", fetchMock);

    const snippets = await searchKnowledge("Kolibri", { topK: 2 });

    expect(fetchMock).toHaveBeenCalledWith("/knowledge/search?q=Kolibri&limit=2", {
      signal: undefined,
    });
    expect(snippets).toHaveLength(1);
    expect(snippets[0]).toMatchObject({ id: "a", title: "Doc", content: "Kolibri description", score: 0.9 });
  });

  it("throws descriptive error when the backend fails", async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: false,
      status: 500,
      statusText: "Server Error",
    });
    vi.stubGlobal("fetch", fetchMock);

    await expect(searchKnowledge("Kolibri")).rejects.toThrow(/500/);
  });

  it("returns an empty list when payload structure is invalid", async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({ invalid: [] }),
    });
    vi.stubGlobal("fetch", fetchMock);

    const snippets = await searchKnowledge("Kolibri");
    expect(snippets).toEqual([]);
  });
});
