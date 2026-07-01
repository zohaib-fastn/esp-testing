// Cloudflare Worker relay: holds the real GitHub token server-side so it
// never ships inside the ESP's publicly-downloadable OTA firmware binary.
//
// Secrets (set via `wrangler secret put <name>`, never in this file):
//   DEVICE_SECRET  - shared secret the ESP sends in X-Device-Secret
//   GITHUB_TOKEN   - fine-grained PAT scoped to this repo only, Actions:write

const OWNER = "zohaib-fastn";
const REPO = "esp-testing";
const WORKFLOW_FILE = "firmware.yml";
const REF = "main";

export default {
  async fetch(request, env) {
    if (request.method !== "POST") {
      return new Response("Method not allowed", { status: 405 });
    }

    const deviceSecret = request.headers.get("X-Device-Secret");
    if (!deviceSecret || deviceSecret !== env.DEVICE_SECRET) {
      return new Response("Unauthorized", { status: 401 });
    }

    const ghResponse = await fetch(
      `https://api.github.com/repos/${OWNER}/${REPO}/actions/workflows/${WORKFLOW_FILE}/dispatches`,
      {
        method: "POST",
        headers: {
          Authorization: `Bearer ${env.GITHUB_TOKEN}`,
          Accept: "application/vnd.github+json",
          "Content-Type": "application/json",
          "User-Agent": "esp-testing-relay",
        },
        body: JSON.stringify({ ref: REF }),
      }
    );

    if (ghResponse.status === 204) {
      return new Response("Workflow triggered", { status: 200 });
    }

    const body = await ghResponse.text();
    return new Response(`GitHub API error: ${ghResponse.status} ${body}`, {
      status: 502,
    });
  },
};
