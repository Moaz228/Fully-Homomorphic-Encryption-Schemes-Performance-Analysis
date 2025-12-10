#!/bin/python3

import pandas
import numpy
import matplotlib.pyplot
from scipy.stats import norm

if __name__ == "__main__":
    data_frame = pandas.read_csv("../build/output_1765307385026743485.csv")
    print(data_frame.columns)
    data = data_frame["inp_8_enc_time"]

    mu, std = norm.fit(data)

    matplotlib.pyplot.hist(data, bins=25, density=True, alpha=0.6, color='b')
    matplotlib.pyplot.hist(data, bins=100, density=True)

    xmin, xmax = matplotlib.pyplot.xlim()
    x = numpy.linspace(xmin, xmax, 100)
    p = norm.pdf(x, mu, std)
    matplotlib.pyplot.plot(x, p, 'r', linewidth=2)

    matplotlib.pyplot.xlabel('Time(ns)')
    matplotlib.pyplot.ylabel('Probability Density')
    # matplotlib.pyplot.title(f'Normal Distribution (μ={mu:.2f}, σ={std:.2f})')
    matplotlib.pyplot.show()
