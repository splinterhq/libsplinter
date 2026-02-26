import basePath from "lume/plugins/base_path.ts";
import metas from "lume/plugins/metas.ts";
import { Options as SitemapOptions, sitemap } from "lume/plugins/sitemap.ts";
import { favicon, Options as FaviconOptions } from "lume/plugins/favicon.ts";
import { merge } from "lume/core/utils/object.ts";
import codeHighlight from "lume/plugins/code_highlight.ts";
import robots from "lume/plugins/robots.ts";

import "lume/types.ts";

export interface Options {
  sitemap?: Partial<SitemapOptions>;
  favicon?: Partial<FaviconOptions>;
}

export const defaults: Options = {
  favicon: {
    input: "uploads/favicon.svg",
  },
};

/** Configure the site */
export default function (userOptions?: Options) {
  const options = merge(defaults, userOptions);

  return (site: Lume.Site) => {
    site
      .use(basePath())
      .use(metas())
      .use(robots({
          rules: [
            {
              userAgent: "*",
              allow: "/",
            },
          ],
      }))
      .use(sitemap(options.sitemap))
      .use(favicon(options.favicon))
      .use(codeHighlight({
          theme: {
          name: "a11y-dark", // The theme name to download
          cssFile: "/style.css", // The destination filename
          placeholder: "/* insert-theme-here */", // Optional placeholder to replace with the theme code
        }
      }))
      .add("uploads")
      .add("style.css");
  };
}
