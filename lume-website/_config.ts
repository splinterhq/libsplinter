import lume from "lume/mod.ts";
import plugins from "./plugins.ts";

const site = lume({
  location: new URL("https://splinter-website.netlify.app"),
  src: "./src",
});

site.use(plugins());

export default site;
