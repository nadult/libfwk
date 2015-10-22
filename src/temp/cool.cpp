void DynamicMesh::makeCool(float tolerance, int max_steps) {
	// TODO: use heap
	std::set<pair<Simplex, Simplex>> elems;
	for(auto vert : verts())
		insert(elems, nearbyPairs(vert, tolerance));
	for(auto edge : edges())
		insert(elems, nearbyEdges(edge, tolerance));

	int merged_count[3] = {0, 0, 0}, initial_count = (int)elems.size();

	while(!elems.empty()) {
		auto pair = *begin(elems);
		elems.erase(begin(elems));

		if(!isValid(pair) || sdistance(pair.first, pair.second) >= tolerance || max_steps <= 0)
			continue;

		if(pair.first.isVertex() && pair.second.isVertex()) {
			auto vert1 = pair.first.asVertex(), vert2 = pair.second.asVertex();
			auto new_vert = merge({vert1, vert2});
			insert(elems, nearbyPairs(new_vert, tolerance));
			merged_count[0]++;
			max_steps--;
		}
		if(pair.first.isEdge() && pair.second.isVertex()) {
			auto edge = pair.first.asEdge();
			auto vert = pair.second.asVertex();
			auto evert = addVertex(closestPoint(segment(edge), point(vert)));
			//			DASSERT(distance(point(evert), point(edge.a)) >= tolerance);
			//			DASSERT(distance(point(evert), point(edge.b)) >= tolerance);

			if(distance(point(evert), point(edge.a)) < tolerance) {
				remove(evert);
				auto new_vert = merge({vert, edge.a});
				insert(elems, nearbyPairs(new_vert, tolerance));
			} else if(distance(point(evert), point(edge.b)) < tolerance) {
				remove(evert);
				auto new_vert = merge({vert, edge.b});
				insert(elems, nearbyPairs(new_vert, tolerance));
			} else {
				split(edge, evert);
				auto new_vert = merge({evert, vert});
				insert(elems, nearbyPairs(new_vert, tolerance));
				insert(elems, nearbyPairs(EdgeId(new_vert, edge.b), tolerance));
				insert(elems, nearbyPairs(EdgeId(edge.a, new_vert), tolerance));
			}

			merged_count[1]++;
			max_steps--;
		}
		if(pair.first.isEdge() && pair.second.isEdge()) {
			auto edge1 = pair.first.asEdge();
			auto edge2 = pair.second.asEdge();
			auto cpoints = closestPoints(segment(edge1), segment(edge2));
			auto vert1 = addVertex(cpoints.first);
			auto vert2 = addVertex(cpoints.second);

			//			DASSERT(distance(point(vert1), point(edge1.a)) >= tolerance);
			//			DASSERT(distance(point(vert1), point(edge1.b)) >= tolerance);
			//			DASSERT(distance(point(vert2), point(edge2.a)) >= tolerance);
			//			DASSERT(distance(point(vert2), point(edge2.b)) >= tolerance);

			split(edge1, vert1);
			split(edge2, vert2);

			auto new_vert = merge({vert1, vert2});
			insert(elems, nearbyPairs(new_vert, tolerance));
			insert(elems, nearbyPairs(EdgeId(new_vert, edge1.b), tolerance));
			insert(elems, nearbyPairs(EdgeId(edge1.a, new_vert), tolerance));
			insert(elems, nearbyPairs(EdgeId(new_vert, edge2.b), tolerance));
			insert(elems, nearbyPairs(EdgeId(edge2.a, new_vert), tolerance));
			merged_count[2]++;
			max_steps--;
		}
	}

	printf("Merged vert/vert: %d vert/edge:%d edge/edge:%d initial:%d\n", merged_count[0],
		   merged_count[1], merged_count[2], initial_count);
}
