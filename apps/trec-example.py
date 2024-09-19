#!/usr/bin/env python3

import sys

def trecs(time, scores):
    total = {}
    for trec in scores:
        total[trec] = 0
        for topic in scores[trec]:
            total[trec] += scores[trec][topic]
        if trec == "4":
            total[trec] /= 49
        else:
            total[trec] /= 50
    trec4 = 0.0
    if "4" in total:
        trec4 = total["4"]
    trec5 = 0.0
    if "5" in total:
        trec5 = total["5"]
    trec6 = 0.0
    if "6" in total:
        trec6 = total["6"]
    trec7 = 0.0
    if "7" in total:
        trec7 = total["7"]
    print(time, trec4, trec5, trec6, trec7)
    sys.stdout.flush()

if __name__ == "__main__":
    first = True;
    scores = {}
    for line in sys.stdin:
        (time, trec, topic, score) = line.rstrip().split()
        time = int(time)
        score = float(score)
        if trec not in scores:
            scores[trec] = {}
        scores[trec][topic] = score
        if (first):
            first = False
            first_time = last_time = time
        if time != last_time:
            last_time = time
            trecs(last_time - first_time, scores)
    if not first:
        trecs(last_time - first_time, scores)
