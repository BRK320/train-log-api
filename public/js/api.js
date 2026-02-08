const API_BASE = "";

async function api(path, { method="GET", body=null, auth=false } = {}) {
  const headers = {};

  if (body !== null) headers["Content-Type"] = "application/json";

  if (auth) {
    const t = localStorage.getItem("token") || "";
    if (!t) throw new Error("Sem token");
    headers["Authorization"] = "Bearer " + t;
  }

  let res;
  try {
    res = await fetch(API_BASE + path, {
      method,
      headers,
      body: body !== null ? JSON.stringify(body) : null
    });
  } catch {
    throw new Error("Não consegui ligar ao servidor.");
  }

  if (res.status === 204) return null;

  const text = await res.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch {}

  if (!res.ok) throw new Error(data?.error || text || `HTTP ${res.status}`);
  return data;
}
