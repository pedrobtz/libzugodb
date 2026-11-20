SELECT like_escape(repeat('a', 29999), repeat('%a', 30000), '');
