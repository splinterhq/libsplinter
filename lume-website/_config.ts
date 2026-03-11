import lume from "lume/mod.ts";
import lumocs from "lumocs/mod.ts";
import plugins from "./_plugins.ts";

const site = lume({
  location: new URL("https://splinterhq.github.io/"),
  src: "./",
});

site.use(lumocs());

site.use(plugins());

export default site;
