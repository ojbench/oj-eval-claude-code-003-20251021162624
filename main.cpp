#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <unordered_map>

using namespace std;

enum Status { ACCEPTED, WRONG_ANSWER, RUNTIME_ERROR, TIME_LIMIT_EXCEED };

struct Submission {
    string problem;
    string team;
    Status status;
    int time;
};

struct ProblemInfo {
    bool solved = false;
    int solve_time = -1;
    int wrong_before = 0;
    int frozen_wrong = 0;
    bool frozen = false;

    int penalty() const {
        return solved ? 20 * wrong_before + solve_time : 0;
    }

    int total_attempts() const {
        return wrong_before + frozen_wrong;
    }
};

struct Team {
    string name;
    unordered_map<string, ProblemInfo> problems;
    int solved = 0;
    int penalty = 0;
    vector<int> solve_times;

    void update(const vector<string>& problem_names) {
        solved = 0;
        penalty = 0;
        solve_times.clear();

        for (const auto& p : problem_names) {
            const auto& prob = problems[p];
            if (prob.solved && !prob.frozen) {
                solved++;
                penalty += prob.penalty();
                solve_times.push_back(prob.solve_time);
            }
        }

        sort(solve_times.begin(), solve_times.end(), greater<int>());
    }

    bool operator<(const Team& other) const {
        if (solved != other.solved) return solved > other.solved;
        if (penalty != other.penalty) return penalty < other.penalty;
        for (size_t i = 0; i < min(solve_times.size(), other.solve_times.size()); i++) {
            if (solve_times[i] != other.solve_times[i]) {
                return solve_times[i] < other.solve_times[i];
            }
        }
        return name < other.name;
    }
};

class ICPC {
    bool started = false;
    bool frozen = false;
    int duration;
    int problem_count;
    vector<string> problems;
    unordered_map<string, Team> teams;
    vector<Submission> submissions;
    vector<Team*> ranking;
    bool dirty = true;

public:
    void add_team(const string& name) {
        if (started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        if (teams.count(name)) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        teams[name] = Team();
        teams[name].name = name;
        cout << "[Info]Add successfully.\n";
    }

    void start(int dur, int count) {
        if (started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        duration = dur;
        problem_count = count;
        started = true;
        for (int i = 0; i < count; i++) {
            problems.push_back(string(1, 'A' + i));
        }
        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem, const string& team,
                const string& status_str, int time) {
        Status status = WRONG_ANSWER;
        if (status_str == "Accepted") status = ACCEPTED;
        else if (status_str == "Wrong_Answer") status = WRONG_ANSWER;
        else if (status_str == "Runtime_Error") status = RUNTIME_ERROR;
        else if (status_str == "Time_Limit_Exceed") status = TIME_LIMIT_EXCEED;

        submissions.push_back({problem, team, status, time});

        Team& t = teams[team];
        ProblemInfo& p = t.problems[problem];

        if (p.solved) return;

        if (status == ACCEPTED) {
            p.solved = true;
            p.solve_time = time;
            if (!frozen) {
                t.update(problems);
                dirty = true;
            }
        } else {
            if (frozen) {
                p.frozen_wrong++;
                p.frozen = true;
            } else {
                p.wrong_before++;
                t.update(problems);
                dirty = true;
            }
        }
    }

    void flush() {
        cout << "[Info]Flush scoreboard.\n";
        update_ranking();
    }

    void freeze() {
        if (frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }
        frozen = true;
        cout << "[Info]Freeze scoreboard.\n";
    }

    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        update_ranking();
        print_board();

        while (true) {
            Team* target = nullptr;
            string target_prob;

            for (int i = ranking.size() - 1; i >= 0; i--) {
                Team* team = ranking[i];
                for (const auto& p : problems) {
                    auto& prob = team->problems[p];
                    if (prob.frozen) {
                        target = team;
                        target_prob = p;
                        break;
                    }
                }
                if (target) break;
            }

            if (!target) break;

            ProblemInfo& prob = target->problems[target_prob];
            prob.frozen = false;
            prob.wrong_before += prob.frozen_wrong;
            prob.frozen_wrong = 0;

            target->update(problems);
            update_ranking();

            int new_rank = -1;
            for (size_t i = 0; i < ranking.size(); i++) {
                if (ranking[i] == target) {
                    new_rank = i + 1;
                    break;
                }
            }

            if (new_rank > 1) {
                Team* replaced = ranking[new_rank - 2];
                cout << target->name << " " << replaced->name << " "
                     << target->solved << " " << target->penalty << "\n";
            }
        }

        frozen = false;
        print_board();
    }

    void query_rank(const string& team) {
        if (!teams.count(team)) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";
        if (frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        update_ranking();
        for (size_t i = 0; i < ranking.size(); i++) {
            if (ranking[i]->name == team) {
                cout << team << " NOW AT RANKING " << (i + 1) << "\n";
                return;
            }
        }
    }

    void query_sub(const string& team, const string& prob_filter,
                   const string& status_filter) {
        if (!teams.count(team)) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it) {
            if (it->team == team) {
                bool prob_ok = (prob_filter == "ALL") || (it->problem == prob_filter);
                bool stat_ok = (status_filter == "ALL") ||
                              (status_str(it->status) == status_filter);

                if (prob_ok && stat_ok) {
                    cout << it->team << " " << it->problem << " "
                         << status_str(it->status) << " " << it->time << "\n";
                    return;
                }
            }
        }

        cout << "Cannot find any submission.\n";
    }

    void end() {
        cout << "[Info]Competition ends.\n";
    }

private:
    void update_ranking() {
        if (!dirty && !frozen) return;

        ranking.clear();
        for (auto& [name, team] : teams) {
            ranking.push_back(&team);
        }

        sort(ranking.begin(), ranking.end(), [](Team* a, Team* b) {
            return *a < *b;
        });

        dirty = false;
    }

    void print_board() {
        for (size_t i = 0; i < ranking.size(); i++) {
            Team* team = ranking[i];
            cout << team->name << " " << (i + 1) << " " << team->solved
                 << " " << team->penalty;

            for (const auto& p : problems) {
                const ProblemInfo& prob = team->problems[p];
                cout << " ";

                if (prob.frozen) {
                    if (prob.wrong_before == 0) {
                        cout << "0/" << prob.frozen_wrong;
                    } else {
                        cout << "-" << prob.wrong_before << "/" << prob.frozen_wrong;
                    }
                } else if (prob.solved) {
                    if (prob.wrong_before == 0) {
                        cout << "+";
                    } else {
                        cout << "+" << prob.wrong_before;
                    }
                } else {
                    int total = prob.total_attempts();
                    if (total == 0) {
                        cout << ".";
                    } else {
                        cout << "-" << total;
                    }
                }
            }
            cout << "\n";
        }
    }

    string status_str(Status s) {
        switch (s) {
            case ACCEPTED: return "Accepted";
            case WRONG_ANSWER: return "Wrong_Answer";
            case RUNTIME_ERROR: return "Runtime_Error";
            case TIME_LIMIT_EXCEED: return "Time_Limit_Exceed";
        }
        return "";
    }
};

int main() {
    ICPC system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if (cmd == "ADDTEAM") {
            string team;
            iss >> team;
            system.add_team(team);
        }
        else if (cmd == "START") {
            string dummy;
            int dur, count;
            iss >> dummy >> dur >> dummy >> count;
            system.start(dur, count);
        }
        else if (cmd == "SUBMIT") {
            string problem, by, team, with, status, at;
            int time;
            iss >> problem >> by >> team >> with >> status >> at >> time;
            system.submit(problem, team, status, time);
        }
        else if (cmd == "FLUSH") {
            system.flush();
        }
        else if (cmd == "FREEZE") {
            system.freeze();
        }
        else if (cmd == "SCROLL") {
            system.scroll();
        }
        else if (cmd == "QUERY_RANKING") {
            string team;
            iss >> team;
            system.query_rank(team);
        }
        else if (cmd == "QUERY_SUBMISSION") {
            string team, where, prob_eq, prob_filter, and_str, stat_eq, stat_filter;
            iss >> team >> where >> prob_eq >> prob_filter >> and_str >> stat_eq >> stat_filter;

            if (prob_eq.find("PROBLEM=") == 0) {
                prob_filter = prob_eq.substr(8);
            }
            if (stat_eq.find("STATUS=") == 0) {
                stat_filter = stat_eq.substr(7);
            }

            system.query_sub(team, prob_filter, stat_filter);
        }
        else if (cmd == "END") {
            system.end();
            break;
        }
    }

    return 0;
}