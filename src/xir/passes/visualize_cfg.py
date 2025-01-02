import graphviz
from sys import argv
import json


def parse_control_flow_graphs_from_xir(xir_file):
    with open(xir_file, 'r') as f:
        prefix = "// CFG = "
        lines = [x[len(prefix):] for line in f.readlines() if (x := line.strip()) and x.startswith(prefix)]
    return [json.loads(line) for line in lines]


def normalize_node_name(node):
    return node.replace("%", "block.")


def visualize_control_flow_graph(filename, graph_desc):
    graph = graphviz.Digraph(format='png')
    nodes = graph_desc['nodes']
    edges = graph_desc['edges']
    dom_edges = graph_desc['dominance_tree']
    frontiers = graph_desc['dominance_frontiers']
    for node in nodes:
        graph.node(node, label=normalize_node_name(node))
    for (source, targets) in edges.items():
        for target in targets:
            graph.edge(source, target)
    for (source, targets) in dom_edges.items():
        for target in targets:
            graph.edge(source, target, color='red')
    for (source, targets) in frontiers.items():
        for target in targets:
            graph.edge(source, target, color='blue', style='dotted')
    func = graph_desc['function'].replace("%", "func.")
    graph.attr(label=f"Control Flow Graph for {func}")
    graph.render(filename)


if __name__ == '__main__':
    if len(argv) < 2:
        print("Usage: visualize_cfg.py <xir_file>")
        exit(1)
    graphs = parse_control_flow_graphs_from_xir(argv[1])
    for i, graph in enumerate(graphs):
        function = graph['function'].replace("%", "func.")
        visualize_control_flow_graph(f"{argv[1]}.{i}", graph)
