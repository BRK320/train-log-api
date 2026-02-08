const $ = (id) => document.getElementById(id);

async function ensureAdmin() {
  if (!getToken()) { location.href = "index.html"; return false; }

  try {
    const data = await me();
    if (data.user.role !== "admin") {
      location.href = "dashboard.html";
      return false;
    }
    return true;
  } catch {
    clearToken();
    location.href = "index.html";
    return false;
  }
}

function setText(id, txt) {
  const el = $(id);
  if (el) el.textContent = txt || "";
}

async function loadExercises() {
  setText("msg", "");
  const list = $("list");
  if (!list) return;

  list.textContent = "a carregar...";

  try {
    const data = await api("/exercises");
    list.innerHTML = "";

    (data || []).forEach(ex => {
      const div = document.createElement("div");
      div.className = "item";
      div.innerHTML = `
        <div><b>#${ex.id}</b> — ${ex.name}</div>
        <div><button class="fillEdit" data-id="${ex.id}" data-name="${ex.name}">Editar</button></div>
      `;
      list.appendChild(div);
    });

    document.querySelectorAll(".fillEdit").forEach(btn => {
      btn.onclick = () => {
        const idEl = $("editId");
        const nameEl = $("editName");
        if (idEl) idEl.value = btn.dataset.id;
        if (nameEl) nameEl.value = btn.dataset.name;
      };
    });
  } catch (e) {
    list.textContent = "";
    setText("msg", e.message);
  }
}

async function createExercise() {
  setText("createOut", "");
  const nameEl = $("createName");
  const name = nameEl ? nameEl.value.trim() : "";
  if (!name) { setText("createOut", "Falta nome"); return; }

  try {
    const data = await api("/exercises", { method: "POST", auth: true, body: { name } });
    setText("createOut", JSON.stringify(data, null, 2));
    if (nameEl) nameEl.value = "";
    await loadExercises();
  } catch (e) {
    setText("createOut", e.message);
  }
}

async function updateExercise() {
  setText("editOut", "");
  const id = parseInt(($("editId")?.value || "0"), 10);
  const name = ($("editName")?.value || "").trim();

  if (!id || id <= 0) { setText("editOut", "ID inválido"); return; }
  if (!name) { setText("editOut", "Falta nome"); return; }

  try {
    const data = await api(`/exercises/${id}`, { method: "PUT", auth: true, body: { name } });
    setText("editOut", JSON.stringify(data, null, 2));
    await loadExercises();
  } catch (e) {
    setText("editOut", e.message);
  }
}

async function deleteExercise() {
  setText("delOut", "");
  const id = parseInt(($("delId")?.value || "0"), 10);
  if (!id || id <= 0) { setText("delOut", "ID inválido"); return; }

  if (!confirm(`Apagar o exercício #${id}?`)) return;

  try {
    await api(`/exercises/${id}`, { method: "DELETE", auth: true });
    setText("delOut", "apagado");
    const delEl = $("delId");
    if (delEl) delEl.value = "";
    await loadExercises();
  } catch (e) {
    setText("delOut", e.message);
  }
}

async function loadUsers() {
  const container = $("usersList");
  if (!container) return;
  container.textContent = "a carregar...";

  try {
    const users = await api("/admin/users", { auth: true });
    container.innerHTML = "";

    if (!users || users.length === 0) {
      container.textContent = "Nenhum utilizador encontrado.";
      return;
    }

    const table = document.createElement("table");
    table.innerHTML = "<thead><tr><th>ID</th><th>Email</th><th>Nome</th><th>Apelido</th><th>Role</th></tr></thead>";
    const tbody = document.createElement("tbody");
    (users || []).forEach(u => {
      const tr = document.createElement("tr");
      tr.innerHTML = `
        <td>${u.id}</td>
        <td>${u.email}</td>
        <td>${u.name}</td>
        <td>${u.surname}</td>
        <td><span class="badge ${u.role}">${u.role}</span></td>
      `;
      tbody.appendChild(tr);
    });
    table.appendChild(tbody);
    container.appendChild(table);
  } catch (e) {
    container.textContent = e.message;
  }
}

async function createUser() {
  setText("createUserOut", "");
  const email = ($("cuEmail")?.value || "").trim();
  const password = ($("cuPass")?.value || "");
  const name = ($("cuName")?.value || "").trim();
  const surname = ($("cuSurname")?.value || "").trim();
  const role = ($("cuRole")?.value || "client");

  if (!email || !password || !name || !surname) {
    setText("createUserOut", "Preenche todos os campos");
    return;
  }

  try {
    const data = await api("/admin/users", {
      method: "POST",
      auth: true,
      body: { email, password, name, surname, role }
    });
    setText("createUserOut", JSON.stringify(data, null, 2));
    if ($("cuEmail")) $("cuEmail").value = "";
    if ($("cuPass")) $("cuPass").value = "";
    if ($("cuName")) $("cuName").value = "";
    if ($("cuSurname")) $("cuSurname").value = "";
    await loadUsers();
  } catch (e) {
    setText("createUserOut", e.message);
  }
}

function wire() {
  const logoutLink = $("logoutLink");
  if (logoutLink) {
    logoutLink.onclick = async (e) => {
      e.preventDefault();
      try { await logout(); } catch {}
      location.href = "index.html";
    };
  }

  const btnReload = $("btnReload");
  if (btnReload) btnReload.onclick = loadExercises;

  const btnCreate = $("btnCreate");
  if (btnCreate) btnCreate.onclick = createExercise;

  const btnEdit = $("btnEdit");
  if (btnEdit) btnEdit.onclick = updateExercise;

  const btnDel = $("btnDel");
  if (btnDel) btnDel.onclick = deleteExercise;

  const btnLoadUsers = $("btnLoadUsers");
  if (btnLoadUsers) btnLoadUsers.onclick = loadUsers;

  const btnCreateUser = $("btnCreateUser");
  if (btnCreateUser) btnCreateUser.onclick = createUser;
}

(async function init() {
  const ok = await ensureAdmin();
  if (!ok) return;
  wire();
  await loadExercises();
  await loadUsers();
})();
