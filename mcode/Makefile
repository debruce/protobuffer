all : m m_anon

m : main.cpp Filer.cpp
	g++ --std=c++17 -o $@ $+

m_anon : m_anon.cpp
	g++ --std=c++17 -o $@ $+