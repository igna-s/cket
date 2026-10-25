// Keep the existing mutable path for values that will be observed, transformed
// or transferred to an observed result. Other known-basis fills can retain a
// computed definition and clean their temporary storage at its scope boundary.
std::set<tree> observed_quantum_outputs;

std::set<tree> observation_reads(tree value) {
    std::set<tree> reads;
    auto inspect = [](tree *node, int *walk, void *data) -> tree {
        auto &reads = *static_cast<std::set<tree> *>(data);
        if (quantum(*node)) {
            reads.insert(*node);
            *walk = 0;
        } else if (quantum_indexed_accesses.count(*node)) {
            const auto &access = quantum_indexed_accesses.at(*node);
            reads.insert(access.elements.begin(), access.elements.end());
            *walk = 0;
        } else if (DECL_P(*node) || TYPE_P(*node))
            *walk = 0;
        return NULL_TREE;
    };
    walk_tree_without_duplicates(&value, inspect, &reads);
    return reads;
}

struct ObservationFlow {
    std::set<tree> outputs, controls;
    std::map<tree, std::set<tree>> sources;
};

tree collect_observation_flow(tree *node, int *walk, void *data) {
    auto &flow = *static_cast<ObservationFlow *>(data);
    tree value = *node;
    if (DECL_P(value) || TYPE_P(value)) {
        *walk = 0;
        return NULL_TREE;
    }
    auto link = [&flow](tree destination, const std::set<tree> &reads) {
        auto &sources = flow.sources[destination];
        sources.insert(reads.begin(), reads.end());
        sources.insert(flow.controls.begin(), flow.controls.end());
    };
    if (TREE_CODE(value) == DECL_EXPR) {
        tree decl = DECL_EXPR_DECL(value);
        if (DECL_INITIAL(decl)) {
            if (quantum(decl))
                link(decl, observation_reads(DECL_INITIAL(decl)));
            walk_tree(&DECL_INITIAL(decl), collect_observation_flow, data, nullptr);
        }
        *walk = 0;
        return NULL_TREE;
    }
    if (TREE_CODE(value) == COND_EXPR) {
        walk_tree(&TREE_OPERAND(value, 0), collect_observation_flow, data, nullptr);
        auto previous = flow.controls;
        auto reads = observation_reads(TREE_OPERAND(value, 0));
        flow.controls.insert(reads.begin(), reads.end());
        walk_tree(&TREE_OPERAND(value, 1), collect_observation_flow, data, nullptr);
        walk_tree(&TREE_OPERAND(value, 2), collect_observation_flow, data, nullptr);
        flow.controls.swap(previous);
        *walk = 0;
        return NULL_TREE;
    }
    if (TREE_CODE(value) == CALL_EXPR) {
        auto kind = intrinsic(value);
        if (kind == "measure" || kind == "reset" || kind == "fourier" ||
            kind == "inverse_fourier" || kind == "search" || kind == "circuit" ||
            kind == "inverse_circuit") {
            for (int i = 0; i < call_expr_nargs(value); ++i) {
                auto reads = observation_reads(CALL_EXPR_ARG(value, i));
                flow.outputs.insert(reads.begin(), reads.end());
            }
        }
    }
    if (quantum_memory_copies.count(value)) {
        for (const auto &entry : quantum_memory_copies.at(value).elements)
            link(entry.first, {entry.second});
    } else if (quantum_memory_sets.count(value) || TREE_CODE(value) == MODIFY_EXPR ||
               TREE_CODE(value) == INIT_EXPR || TREE_CODE(value) == PREINCREMENT_EXPR ||
               TREE_CODE(value) == POSTINCREMENT_EXPR || TREE_CODE(value) == PREDECREMENT_EXPR ||
               TREE_CODE(value) == POSTDECREMENT_EXPR) {
        auto access = quantum_accesses(value);
        for (tree destination : access.writes)
            link(destination, access.reads);
    }
    return NULL_TREE;
}

std::set<tree> observed_storage_dependencies(tree body) {
    ObservationFlow flow;
    walk_tree(&body, collect_observation_flow, &flow, nullptr);
    std::vector<tree> pending(flow.outputs.begin(), flow.outputs.end());
    while (!pending.empty()) {
        tree destination = pending.back();
        pending.pop_back();
        for (tree source : flow.sources[destination])
            if (flow.outputs.insert(source).second)
                pending.push_back(source);
    }
    return flow.outputs;
}

// Reuse the same physical storage, with a real saved inverse. The old basis
// value is known and independent, so unprepare/release is unitary. Reallocate
// the clean slot before computing the selected value; no reset is introduced.
void compute_known_storage(Variable &var,
                           const std::string &expr,
                           const std::set<unsigned> &deps,
                           location_t loc,
                           tree *list) {
    std::string inactive =
        "{\"op\":\"const\",\"bits\":" + std::to_string(var.width) +
        ",\"signed\":false,\"value\":" + std::to_string(var.basis_known ? var.lower : 0) + '}';
    inactive = cast_result(inactive, var);
    std::set<unsigned> sources = deps;
    std::string value = select_controls(expr, inactive, var.width, var.is_signed, sources);
    std::set<unsigned> parameters;
    parameters.swap(active_cpu_parameters);
    append(list, emit_operation("{\"op\":\"qretain\",\"id\":" + std::to_string(var.id) + '}', loc));
    append(list,
           emit_operation("{\"op\":\"qhistory_end\",\"id\":" + std::to_string(var.id) + '}', loc));
    append(list,
           emit_operation("{\"op\":\"qalloc\",\"id\":" + std::to_string(var.id) +
                              ",\"width\":" + std::to_string(var.width) + ",\"signed\":" +
                              (var.is_signed ? "true" : "false") + ",\"ancilla\":true}",
                          loc));
    active_cpu_parameters.swap(parameters);
    append(list,
           emit_operation("{\"op\":\"qcompute\",\"id\":" + std::to_string(var.id) +
                              ",\"expr\":" + value + '}',
                          loc));
    var.derived = true;
    var.optional_definition = !cpu_branches.empty();
    // The other CPU arm may keep the known root in this same public slot.
    // Disposal follows the actual path's optional saved definition.
    var.runtime_history |= !cpu_branches.empty();
    var.history = false;
    var.dependencies = sources;
    var.creation = value;
    var.range_max = var.width == 64 ? UINT64_MAX : ((uint64_t{1} << var.width) - 1);
    var.measured = var.clean = var.basis_known = var.pristine_uniform = false;
    ++var.epoch;
}
