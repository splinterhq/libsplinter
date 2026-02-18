# Semantic Retrieval Augmented Generation With Lume

This is a project to bring [Tieto][1]'s flat-file embedding storage and 
semantic matching capabilities to [Lume][2]'s powerful document processing,
generation capabilities and data model in order to produce a semantic
data store that runs happily at the (freemium) edge with few resources.

That sounds like a lot, so it's easier to describe by telling you what
it will let you do instead:

 - Add true semantic search into your document site or blog that you
 can fine-tune; use a free Nomic Atlas key for embedding, or (with a VPS
 and 256 MB of RAM) do your own embeddings. Become your own limited
 "Algolia" 

 - Provide verbatim, semantic-accessible of LLM conversations through 
 [MCP][3] interaction (using Lume's [router middleware][4])

 - Provide LLMs semantically-accessible short-term memory that you host
 through MCP.

 - Easily create "Librarian" agents to narrate any given text corpus, 
 useful for corporate assistants. 

Because Tieto adds no dependencies on top of Lume, it doesn't alter the 
existing SOC footprint for most companies, so it's an ideal RAG for teams 
that just need to have access to something that works (runs fine on an 
old 13 chrome book, Nomic Text v2 1.3B model included!)

## Requirements

If you just want ***semantic search***, meaning you want something like a 
basic version of Algolia but you own 100% of the code:

 - You will need to have a free [Nomic Atlas][5] key, or the ability to run 
 the open source [Nomic Text v2][5] model. Nomic Text can be run using 
 a quantized (`.gguf`) version of the model with [llama.cpp][6] to serve it 
 at under 300MB resident size. I recommend this route if you can afford it.

 - You will need to be able to host a Lume site on Deno Deploy or a VPS. If 
 you have no money for your project, you can use the free versions and get
 a free Nomic key. If you cen afford a VPS, you can host embedding and
 even completion yourself. It's up to you and your budget.

 If all you want is semantic search and some MCP endpoints, all you need is 
 access to embeddings, which can be obtained at no cost.

 ## Retrieval-only by default. BYOM for completion, too. 

If you also want ***completion***, as in ***RAG*** (retrieval-augmented 
generation), then you will also need to be able to provide a completion 
model key and provider (Featherless, OpenRouter, OpenAI, Anthropic, Etc), 
or host the completion model along with the embedding model.

## Current Status

Still in development and not yet functional.

 [1]: https://github.com/timthepost/tieto
 [2]: https://github.com/lumeland/lume
 [3]: https://modelcontextprotocol.io/docs/getting-started/intro
 [4]: https://lume.land/plugins/router/
 [5]: https://huggingface.co/nomic-ai/nomic-embed-text-v2-moe
 [6]: https://github.com/ggml-org/llama.cpp
 