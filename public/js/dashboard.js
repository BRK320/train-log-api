const $ = (id) => document.getElementById(id);

async function init() {
  if (!getToken()) { location.href = "index.html"; return; }

  try {
    const data = await me();
    const role = data.user.role;
    $("roleBadge").textContent = role;
    $("roleBadge").className = "badge " + role;
    $("userName").textContent = data.user.name + " " + data.user.surname;

    if (role === "admin") $("adminLink").style.display = "inline";
  } catch {
    clearToken();
    location.href = "index.html";
  }

  await loadExercises();
  await loadWorkouts();
}

$("logoutLink").onclick = async (e) => {
  e.preventDefault();
  try { await logout(); } catch {}
  location.href = "index.html";
};

async function loadExercises() {
  $("exOut").textContent = "a carregar...";
  try {
    const ex = await api("/exercises");
    $("exOut").innerHTML = "";

    if (!ex || ex.length === 0) {
      $("exOut").textContent = "Nenhum exercício encontrado.";
      return;
    }

    const table = document.createElement("table");
    table.innerHTML = "<thead><tr><th>ID</th><th>Nome</th></tr></thead>";
    const tbody = document.createElement("tbody");
    ex.forEach(e => {
      const tr = document.createElement("tr");
      tr.innerHTML = `<td>${e.id}</td><td>${e.name}</td>`;
      tbody.appendChild(tr);
    });
    table.appendChild(tbody);
    $("exOut").appendChild(table);
  } catch (e) {
    $("exOut").textContent = e.message;
  }
}

$("btnLoadEx").onclick = loadExercises;

$("btnCreateW").onclick = async () => {
  $("workoutMsg").textContent = "";
  try {
    const w = await api("/workouts", { method:"POST", auth:true });
    $("workoutMsg").textContent = "Workout criado com id: " + w.id;
    $("workoutMsg").className = "success";
    await loadWorkouts();
  } catch (e) {
    $("workoutMsg").textContent = e.message;
    $("workoutMsg").className = "error";
  }
};

async function loadWorkouts() {
  const container = $("workoutsList");
  if (!container) return;
  container.textContent = "a carregar...";

  try {
    const workouts = await api("/workouts", { auth:true });
    container.innerHTML = "";

    if (!workouts || workouts.length === 0) {
      container.textContent = "Nenhum workout encontrado.";
      return;
    }

    workouts.forEach(w => {
      const div = document.createElement("div");
      div.className = "workout-item";
      div.innerHTML = `
        <div class="workout-header">
          <span><b>Workout #${w.id}</b> — ${w.created_at || ""}</span>
          <span>
            <button class="small" onclick="viewWorkout(${w.id})">Ver</button>
            <button class="small danger-btn" onclick="deleteWorkout(${w.id})">Apagar</button>
          </span>
        </div>
      `;
      container.appendChild(div);
    });
  } catch (e) {
    container.textContent = e.message;
  }
}

async function viewWorkout(id) {
  const detail = $("workoutDetail");
  if (!detail) return;
  detail.textContent = "a carregar...";

  try {
    const w = await api(`/workouts/${id}`, { auth:true });
    detail.innerHTML = "";

    const header = document.createElement("div");
    header.innerHTML = `<h4>Workout #${w.id} — ${w.created_at || ""}</h4>`;
    detail.appendChild(header);

    const sets = w.sets || [];
    if (sets.length === 0) {
      const p = document.createElement("p");
      p.textContent = "Sem sets.";
      p.className = "muted";
      detail.appendChild(p);
    } else {
      const table = document.createElement("table");
      table.innerHTML = "<thead><tr><th>Set ID</th><th>Exercício</th><th>Reps</th><th>Peso (kg)</th><th>Ações</th></tr></thead>";
      const tbody = document.createElement("tbody");
      sets.forEach(s => {
        const tr = document.createElement("tr");
        tr.innerHTML = `
          <td>${s.id}</td>
          <td>${s.exercise_name || s.exercise_id}</td>
          <td>${s.reps}</td>
          <td>${s.weight}</td>
          <td>
            <button class="small" onclick="fillEditSet(${w.id}, ${s.id}, ${s.exercise_id}, ${s.reps}, ${s.weight})">Editar</button>
            <button class="small danger-btn" onclick="deleteSet(${w.id}, ${s.id})">Apagar</button>
          </td>
        `;
        tbody.appendChild(tr);
      });
      table.appendChild(tbody);
      detail.appendChild(table);
    }

    $("wId").value = w.id;
  } catch (e) {
    detail.textContent = e.message;
  }
}

async function deleteWorkout(id) {
  if (!confirm(`Apagar workout #${id}?`)) return;
  try {
    await api(`/workouts/${id}`, { method:"DELETE", auth:true });
    await loadWorkouts();
    const detail = $("workoutDetail");
    if (detail) detail.innerHTML = "";
  } catch (e) {
    alert(e.message);
  }
}

$("btnAddSet").onclick = async () => {
  $("setMsg").textContent = "";
  try {
    const workoutId = $("wId").value.trim();
    if (!workoutId) throw new Error("Falta workout_id");

    const payload = {
      exercise_id: Number($("exId").value),
      reps: Number($("reps").value),
      weight: Number($("weight").value)
    };

    await api(`/workouts/${workoutId}/sets`, { method:"POST", auth:true, body: payload });
    $("setMsg").textContent = "Set adicionado";
    $("setMsg").className = "success";
    await viewWorkout(Number(workoutId));
  } catch (e) {
    $("setMsg").textContent = e.message;
    $("setMsg").className = "error";
  }
};

function fillEditSet(workoutId, setId, exerciseId, reps, weight) {
  $("editSetWorkoutId").value = workoutId;
  $("editSetId").value = setId;
  $("editSetExId").value = exerciseId;
  $("editSetReps").value = reps;
  $("editSetWeight").value = weight;
  $("editSetCard").style.display = "block";
}

$("btnUpdateSet").onclick = async () => {
  $("editSetMsg").textContent = "";
  try {
    const workoutId = $("editSetWorkoutId").value.trim();
    const setId = $("editSetId").value.trim();

    const payload = {
      exercise_id: Number($("editSetExId").value),
      reps: Number($("editSetReps").value),
      weight: Number($("editSetWeight").value)
    };

    await api(`/workouts/${workoutId}/sets/${setId}`, { method:"PUT", auth:true, body: payload });
    $("editSetMsg").textContent = "Set atualizado";
    $("editSetMsg").className = "success";
    $("editSetCard").style.display = "none";
    await viewWorkout(Number(workoutId));
  } catch (e) {
    $("editSetMsg").textContent = e.message;
    $("editSetMsg").className = "error";
  }
};

$("btnCancelEditSet").onclick = () => {
  $("editSetCard").style.display = "none";
};

async function deleteSet(workoutId, setId) {
  if (!confirm(`Apagar set #${setId}?`)) return;
  try {
    await api(`/workouts/${workoutId}/sets/${setId}`, { method:"DELETE", auth:true });
    await viewWorkout(workoutId);
  } catch (e) {
    alert(e.message);
  }
}

$("btnLoadVolume").onclick = async () => {
  $("volumeOut").textContent = "a carregar...";
  try {
    const data = await api("/stats/volume", { auth:true });
    $("volumeOut").textContent = JSON.stringify(data, null, 2);
  } catch (e) {
    $("volumeOut").textContent = e.message;
  }
};

$("btnLoadPRs").onclick = async () => {
  $("prsOut").textContent = "a carregar...";
  try {
    const data = await api("/stats/prs", { auth:true });
    $("prsOut").textContent = JSON.stringify(data, null, 2);
  } catch (e) {
    $("prsOut").textContent = e.message;
  }
};

$("btnRefreshWorkouts").onclick = loadWorkouts;

init();
