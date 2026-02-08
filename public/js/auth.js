function setToken(t) { localStorage.setItem("token", t); }
function getToken() { return localStorage.getItem("token") || ""; }
function clearToken() { localStorage.removeItem("token"); }

async function login(email, password) {
  const data = await api("/login", { method:"POST", body:{ email, password } });
  setToken(data.token);
  return data;
}

async function signup(email, password, name, surname) {
  const data = await api("/signup", { method:"POST", body:{ email, password, name, surname } });
  setToken(data.token);
  return data;
}

async function me() {
  return api("/me", { auth:true });
}

async function logout() {
  await api("/logout", { method:"POST", auth:true });
  clearToken();
}
