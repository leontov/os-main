export interface KnowledgeSnippet {
  id: string;
  title: string;
  content: string;
  score: number;
  source?: string;
}

export interface KnowledgeSearchResponse {
  snippets: KnowledgeSnippet[];
}
