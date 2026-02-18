// metadata‑aware RAG and retrieval with safe filtering & debug
// uses cosine similarity, file-file topics and JSONL sections.
// Scales easily over local or network storage.
// ============================================================

import { walk } from "https://deno.land/std@0.204.0/fs/walk.ts";
import { join } from "https://deno.land/std@0.204.0/path/mod.ts";
import { extract } from "https://deno.land/std@0.204.0/front_matter/yaml.ts";

// for filter parsing
type Op = "=" | ">=" | "<=" | ">" | "<" | "in";
interface Filter {
  key: string;
  op: Op;
  value: string | string[];
}

interface TietoConfig {
  minSimilarityThreshold?: number;
  debug?: boolean;
  embeddingUrl?: string;
  completionUrl?: string;
  apiKey?: string;
  chunkSize?: number;
  maxResults?: number;
  completionParams?: {
    temperature?: number;
    n_predict?: number;
    max_tokens?: number;
  };
}

interface Chunk {
  text: string;
  embedding: number[];
  meta: Record<string, unknown>;
}

interface ScoredChunk extends Chunk {
  score: number;
}

export class Tieto {
  private config: Required<TietoConfig>;

  constructor(config: TietoConfig = {}) {
    this.config = {
      minSimilarityThreshold: config.minSimilarityThreshold ?? 0.42,
      debug: config.debug ??
        (Deno.args.includes("--debug") || Deno.env.get("DEBUG") === "1"),
      embeddingUrl: config.embeddingUrl ?? Deno.env.get("EMBEDDING_URL") ??
        "http://localhost:8080/v1/embeddings",
      completionUrl: config.completionUrl ??
        Deno.env.get("RAG_COMPLETION_URL") ?? "",
      apiKey: config.apiKey ?? Deno.env.get("API_KEY") ?? "",
      chunkSize: config.chunkSize ?? 3,
      maxResults: config.maxResults ?? 3,
      completionParams: {
        temperature: 0,
        n_predict: 128,
        max_tokens: 500,
        ...config.completionParams,
      },
    };
  }

  private logDebug(...args: unknown[]) {
    if (this.config.debug) console.log("= [debug]", ...args);
  }

  // extremely fast comparison
  private cosineSimilarity(a: number[], b: number[]): number {
    const dot = a.reduce((s, ai, i) => s + ai * b[i], 0);
    const na = Math.hypot(...a);
    const nb = Math.hypot(...b);
    return dot / (na * nb);
  }

  // reference for how magnitude would affect a query
  // can also be a secondary signal for certain uses.
  private euclideanDistance(a: number[], b: number[]): number {
    if (a.length !== b.length) {
      throw new Error("Vectors must have the same dimension.");
    }
    const sumOfSquaredDifferences = a.reduce((sum, ai, i) => {
      const difference = ai - b[i];
      return sum + difference * difference;
    }, 0);
    return Math.sqrt(sumOfSquaredDifferences);
  }

  // You can modify this to use a third-party embedding model, if you
  // need to.
  async embed(text: string): Promise<Float32Array> {
    const headers: Record<string, string> = {
      "Content-Type": "application/json",
    };
    if (this.config.apiKey) {
      headers["Authorization"] = `Bearer ${this.config.apiKey}`;
    }

    const response = await fetch(this.config.embeddingUrl, {
      method: "POST",
      headers,
      body: JSON.stringify({ input: text }),
    });

    if (!response.ok) {
      const errText = await response.text();
      throw new Error(
        `Embedding request failed: ${response.status} ${errText}`,
      );
    }

    const json = await response.json();

    if (
      !json.data ||
      !Array.isArray(json.data) ||
      !json.data[0]?.embedding ||
      !Array.isArray(json.data[0].embedding)
    ) {
      throw new Error("Embedding output malformed or missing");
    }

    return new Float32Array(json.data[0].embedding);
  }

  async ingest(path: string): Promise<void> {
    const raw = await Deno.readTextFile(path);
    const { attrs: meta, body } = extract(raw);
    const lines = body.split("\n").map((l) => l.trim()).filter(Boolean);

    const chunks: Chunk[] = [];

    for (let i = 0; i < lines.length; i += this.config.chunkSize) {
      const text = lines.slice(i, i + this.config.chunkSize).join("\n");
      const vec = Array.from(await this.embed(text));
      chunks.push({ text, embedding: vec, meta });
    }

    const [, topic, nameTxt] = path.split("/");
    const out = `topics/${topic}/memory/${nameTxt.replace(/\.txt$/, ".jsonl")}`;
    await Deno.writeTextFile(
      out,
      chunks.map((c) => JSON.stringify(c)).join("\n"),
    );
    this.logDebug(`✅ Ingested → ${out}`);
  }

  parseFilters(args: string[]): Filter[] {
    const filters: Filter[] = [];
    for (let i = 0; i < args.length; i++) {
      let expr: string | undefined;
      // styles:  --filter key=val   OR  --filter=key=val
      if (args[i] === "--filter") {
        expr = args[i + 1];
        i++; // skip value token
      } else if (args[i].startsWith("--filter=")) {
        expr = args[i].slice("--filter=".length);
      }
      if (!expr) continue;

      const match = expr.match(/^([^\s><=]+)\s*(>=|<=|=|>|<|in)\s*(.+)$/);
      if (!match) {
        this.logDebug(`⚠️  Ignoring bad filter expression: '${expr}'`);
        continue;
      }
      const [, key, op, valRaw] = match as [string, string, Op, string];
      const value: string | string[] = op === "in"
        ? valRaw.split(",").map((s) => s.trim())
        : valRaw.trim();
      filters.push({ key, op, value });
    }
    this.logDebug("Parsed filters", filters);
    return filters;
  }

  private satisfies(meta: Record<string, unknown>, f: Filter): boolean {
    const actualRaw = meta?.[f.key];
    if (actualRaw === undefined || actualRaw === null) return false;

    // convert arrays, objects → string consistently for comparison
    const actualStr = Array.isArray(actualRaw)
      ? actualRaw.join(",")
      : actualRaw.toString();

    if (f.op === "=") return actualStr === f.value;
    if (f.op === "in") {
      const list = Array.isArray(f.value)
        ? f.value
        : (f.value as string).split(",");
      return list.includes(actualStr);
    }

    // numeric or date compare
    const left = Number(actualStr) || Date.parse(actualStr);
    const right = Number(f.value as string) || Date.parse(f.value as string);
    if (isNaN(left) || isNaN(right)) return false;

    switch (f.op) {
      case ">=":
        return left >= right;
      case "<=":
        return left <= right;
      case ">":
        return left > right;
      case "<":
        return left < right;
    }

    // default
    return false;
  }

  async search(
    topic: string,
    question: string,
    filters: Filter[] = [],
  ): Promise<ScoredChunk[]> {
    const memDir = join("topics", topic, "memory");
    const chunks: Chunk[] = [];

    for await (
      const file of walk(memDir, { exts: [".jsonl"], includeDirs: false })
    ) {
      const lines = (await Deno.readTextFile(file.path)).trim().split("\n");
      for (const line of lines) {
        const c = JSON.parse(line);
        if (filters.every((f) => this.satisfies(c.meta ?? {}, f))) {
          chunks.push(c);
        } else {
          this.logDebug("⛔ Excluded by filter:", c.meta);
        }
      }
    }

    if (!chunks.length) {
      this.logDebug("⚠️  No data matched filters", filters);
      return [];
    }

    const qVec = Array.from(await this.embed(question));
    const scored = chunks.map((c) => (
      { ...c, score: this.cosineSimilarity(c.embedding, qVec) }
    ))
      .sort((a, b) => b.score - a.score)
      .slice(0, this.config.maxResults);

    this.logDebug(
      "Query: minimum score for inclusion is ",
      this.config.minSimilarityThreshold,
    );
    this.logDebug("Query: winning score from memory was ", scored[0]?.score);

    if (this.config.debug) {
      for (const element of scored) {
        console.log("===");
        console.log(
          "= Text: ",
          '"' + element.text.replace(/\n/g, "\\n").replace(/\r/g, "\\r") + '"',
        );
        console.log("= Cosine Similarity Score: ", element.score);
        console.log(
          "= Euclidean Distance From Embedded Input: ",
          this.euclideanDistance(qVec, element.embedding),
        );
      }
      console.log("===");
    }

    return scored.filter((chunk) =>
      chunk.score >= this.config.minSimilarityThreshold
    );
  }

  buildPrompt(context: string, question: string): string {
    return `Use the information between the dashes "---" to answer the question that follows:\n\n---\n\n${context}\n\n---\n\nQuestion: ${question}`;
  }

  async complete(prompt: string): Promise<string> {
    if (!this.config.completionUrl) {
      return prompt; // Return prompt if no completion URL configured
    }

    const headers: Record<string, string> = {
      "Content-Type": "application/json",
    };
    if (this.config.apiKey) {
      headers["Authorization"] = `Bearer ${this.config.apiKey}`;
    }

    // Support different API formats
    const body = this.config.completionUrl.includes("anthropic") ||
        this.config.completionUrl.includes("openai")
      ? JSON.stringify({
        model: "claude-3-haiku-20240307", // or configurable
        messages: [{ role: "user", content: prompt }],
        max_tokens: this.config.completionParams.max_tokens,
        temperature: this.config.completionParams.temperature,
      })
      : JSON.stringify({
        prompt,
        temperature: this.config.completionParams.temperature,
        n_predict: this.config.completionParams.n_predict,
      });

    const res = await fetch(this.config.completionUrl, {
      method: "POST",
      headers,
      body,
    });

    if (!res.ok) {
      throw new Error(`Completion request failed: ${res.status}`);
    }

    const json = await res.json();

    // Handle different response formats
    if (json.choices && json.choices[0]?.message?.content) {
      return json.choices[0].message.content.trim(); // OpenAI/Anthropic format
    } else if (json.content) {
      return json.content.trim(); // llama.cpp format
    }

    return "";
  }

  /**
   * Main query method - searches for relevant chunks and optionally generates completion
   */
  async query(
    topic: string,
    question: string,
    filters: Filter[] = [],
    returnRaw = false,
  ): Promise<
    string | { chunks: ScoredChunk[]; response?: string; prompt?: string }
  > {
    const chunks = await this.search(topic, question, filters);

    if (!chunks.length) {
      const msg = "No relevant chunks found above similarity threshold";
      this.logDebug("⚠️  " + msg);
      return returnRaw ? { chunks: [] } : msg;
    }

    const context = chunks.map((c) => c.text).join("\n\n");
    const prompt = this.buildPrompt(context, question);

    if (returnRaw) {
      const response = this.config.completionUrl
        ? await this.complete(prompt)
        : undefined;
      return { chunks, response, prompt };
    }

    if (!this.config.completionUrl) {
      console.log(prompt);
      return prompt;
    }

    const response = await this.complete(prompt);
    console.log(response);
    return response;
  }

  // Utility methods for external integrations
  getConfig(): Required<TietoConfig> {
    return { ...this.config };
  }

  updateConfig(updates: Partial<TietoConfig>): void {
    this.config = { ...this.config, ...updates };
  }
}
