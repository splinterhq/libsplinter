import cacheBusting from "lume/middlewares/cache_busting.ts";
import lume from "lume/mod.ts";
import plugins from "./plugins.ts";

const site = lume({
  src: "./src",
  location: new URL("https://splinter.timthepost.com"),
  server: {
    middlewares: [
      cacheBusting(),
    ],
  },
});

site.use(plugins());

export default site;
