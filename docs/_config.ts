import lume from "lume/mod.ts";
import theme from "theme/mod.ts";
import plugins from "./_plugins.ts";

const site = lume({
    location: new URL("https://splinterhq.github.io"),
});

site.use(theme());
site.use(plugins());
export default site;
