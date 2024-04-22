#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include "misc_utils.h"

#include <array>

/* This is a structure that will wrap around an object that can be interogated in the form:
map_data[i][j] and the cost to travel from two adjiacent cells has to be the difference:
destination - origin.
    
    matrix_graph_wraper_t() - the constructor:
        - map_data - a pointer as a reference to the object
        - max_lines - the maximum number for the i coordinate
        - max_cols - the maximum number for the j coordinate
    
    cost_t - the deduced cost for final path
    node_t - the deduced node
*/

#define PATH_FINDING_FLAG_DIAG_ENABLE 1

template <size_t graph_flags, typename matrix_type_t, typename _cost_t = float>
struct matrix_graph_wraper_t {
    const static constexpr uint32_t neigh_cnt = graph_flags & PATH_FINDING_FLAG_DIAG_ENABLE ? 8 : 4;

    struct node_t {
        int32_t x;
        int32_t y;

        bool operator < (const node_t& oth) const {
            return  x < oth.x ? true :
                    x > oth.x ? false :
                    y < oth.y ? true : false;
        }

        bool operator == (const node_t& oth) const {
            return x == oth.x && y == oth.y;
        }
    };

    matrix_type_t &map_data;
    int32_t max_lines;
    int32_t max_cols;

    using cost_t = _cost_t;

    matrix_graph_wraper_t(matrix_type_t &map_data, uint32_t max_lines, uint32_t max_cols)
    : map_data(map_data), max_lines(max_lines), max_cols(max_cols)
    {}

    std::vector<std::pair<node_t, cost_t>> neighbors(const node_t& node) const {
        std::vector<std::pair<node_t, cost_t>> ret;
        static_assert(neigh_cnt == 4 || neigh_cnt == 8);

        int32_t i = node.y;
        int32_t j = node.x;
        int32_t ml = max_lines;
        int32_t mc = max_cols;
        auto &m = map_data;
        if constexpr (neigh_cnt == 4) {
            if (i-1 >= 0 ) ret.push_back({ node_t{j  , i-1}, abs(m[i-1][j  ] - m[i][j]) });
            if (j+1 <  mc) ret.push_back({ node_t{j+1, i  }, abs(m[i  ][j+1] - m[i][j]) });
            if (i+1 <  ml) ret.push_back({ node_t{j  , i+1}, abs(m[i+1][j  ] - m[i][j]) });
            if (j-1 >= 0 ) ret.push_back({ node_t{j-1, i  }, abs(m[i  ][j-1] - m[i][j]) });
            return ret;
        }
        else if constexpr (neigh_cnt == 8) {
            if (i-1 >= 0  && j-1 >= 0 ) ret.push_back({ node_t{j-1, i-1}, abs(m[i-1][j-1] - m[i][j]) });
            if (i-1 >= 0              ) ret.push_back({ node_t{j  , i-1}, abs(m[i-1][j  ] - m[i][j]) });
            if (i-1 >= 0  && j+1 <  mc) ret.push_back({ node_t{j+1, i-1}, abs(m[i-1][j+1] - m[i][j]) });
            if (j+1 <  mc             ) ret.push_back({ node_t{j+1, i  }, abs(m[i  ][j+1] - m[i][j]) });
            if (i+1 <  ml && j+1 <  mc) ret.push_back({ node_t{j+1, i+1}, abs(m[i+1][j+1] - m[i][j]) });
            if (i+1 <  ml             ) ret.push_back({ node_t{j  , i+1}, abs(m[i+1][j  ] - m[i][j]) });
            if (i+1 <  ml && j-1 >  0 ) ret.push_back({ node_t{j-1, i+1}, abs(m[i+1][j-1] - m[i][j]) });
            if (j-1 >= 0              ) ret.push_back({ node_t{j-1, i  }, abs(m[i  ][j-1] - m[i][j]) });
            return ret;
        }
    }

    static auto get_heuristic(const node_t &goal) {
        // heuristics from here: https://github.com/riscy/a_star_on_grids
        static_assert(neigh_cnt == 4 || neigh_cnt == 8);

        const double C = 1.0;
        const double B = sqrt(2) - 1;
        if constexpr (neigh_cnt == 4) {
            return [goal, C](const node_t& node) -> cost_t {
                int32_t dx = abs(node.x - goal.x);
                int32_t dy = abs(node.y - goal.y);

                return cost_t(C * abs(dx - dy));
            };
        }
        else if constexpr (neigh_cnt == 8) {
            return [goal, C, B](const node_t& node) -> cost_t {
                int32_t dx = abs(node.x - goal.x);
                int32_t dy = abs(node.y - goal.y);

                if (dx > dy) {
                    return cost_t(C*dx + B*dy);
                }
                else {
                    return cost_t(C*dy + B*dx);
                }
            };
        }
    }

    using heuristic_t = decltype(get_heuristic(*(node_t *)NULL));
};

// used this https://www.redblobgames.com/pathfinding/a-star/implementation.html
// and wikipedia https://en.wikipedia.org/wiki/A*_search_algorithm
template <typename heuristic_t, typename cost_t, typename graph_t, typename node_t>
std::vector<node_t> a_star_path(const graph_t& graph, const node_t& start, const node_t& goal,
        const heuristic_t& heuristic)
{
    struct node_cost_t {
        node_t node;
        cost_t cost;
    };

    auto compare_fn = [](const node_cost_t& a, const node_cost_t& b) {
        return a.cost < b.cost;
    };

    /* the set of nodes that need to be analyzed */
    std::priority_queue<node_cost_t, std::vector<node_cost_t>, decltype(compare_fn)>
            open_set(compare_fn);

    /* the previous node regarding the path cost */
    std::map<node_t, node_t> node_prev;

    /* the cost of the path to the given node */
    std::map<node_t, cost_t> g_score;

    open_set.push({start, heuristic(start)});
    g_score[start] = cost_t{0};

    while (open_set.size()) {
        auto [curr_node, cost] = open_set.top();
        open_set.pop();

        if (curr_node == goal) {
            std::vector<node_t> ret;
            ret.push_back(curr_node);
            while (HAS(node_prev, curr_node)) {
                curr_node = node_prev[curr_node];
                ret.push_back(curr_node);
            }
            return ret;
        }

        /* TODO: Test adding a map for nodes to disccard them when seeing them in the queue with
        larger distance than on previous tries, that is to minimize the cost of analyzing the node
        again. */

        auto neighbors_dist = graph.neighbors(curr_node);
        for (auto &[neigh, distance] : neighbors_dist) {
            cost_t new_score = g_score[curr_node] + distance;
            if (!HAS(g_score, neigh) || new_score < g_score[neigh]) {
                node_prev[neigh] = curr_node;
                g_score[neigh] = new_score;
                DBG("new score for [%d %d] is: %f", neigh.x, neigh.y, new_score);

                // we may push the same node multiple times, but we do that anyway, see:
                // UTCS Technical Report TR-07-54
                open_set.push({neigh, new_score + heuristic(neigh)});
            }
        }
    }
    return {};
}

#endif
