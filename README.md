# background_index
A follow up to the sorted_vector timing example.
An example container that uses a background thread to create a fast-index for a large std::map
This should hopefully allow faster access if the index is up to date, but will largely not negatively impact the speed if the index is not available.
