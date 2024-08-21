#!/usr/bin/env python3

import sys

if __name__ == "__main__":
    first = True;
    scores = {}
    for line in sys.stdin:
        (time, n, topic, score) = line.rstrip().split()
        time = int(time)
        n = float(n)
        score = float(score)
        scores[topic] = score
        if (first):
            first = False
            first_time = last_time = time
        if time != last_time:
            last_time = time
            total = 0.0;
            for t in scores:
                total += scores[t];
            if n == 0:
              print(last_time - first_time, 0)
            else:
              print(last_time - first_time, total/n)
        sys.stdout.flush()
    if not first:
        total = 0;
        for topic in scores:
            total += scores[topic];
        if n == 0:
          print(last_time - first_time, 0)
        else:
          print(last_time - first_time, total/n)
        sys.stdout.flush()
