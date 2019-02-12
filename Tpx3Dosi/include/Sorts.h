#pragma once

#include <thread>
#include <vector>

template<class TL, typename TD>//TL = Type list; TD = Type data
void BubbleSort_R(TL * list, size_t startPos, size_t endPos)
{
	/*for (size_t i = endPos; i >= startPos; i--)
	{
		for (size_t j = (i > 0) ? i - 1 : 0; j >= startPos; j--)
		{
			if (*(*list)[i] <= *(*list)[j])
			{
				TD* temp = (*list)[i];
				(*list)[i] = (*list)[j + 1];
				(*list)[j + 1] = temp;
				break;
			}
		}
	}*/

	for (size_t n = startPos; n <= endPos; n++)
	{
		for (size_t i = endPos; i > n; --i)
		{
			if ((*list)[i] < (*list)[i-1])
			{
				TD temp = (*list)[i];
				(*list)[i] = (*list)[i - 1];
				(*list)[i - 1] = temp;
			}
		}
	}
}

template<class TL, typename TD>//TL = Type list; TD = Type data
void BubbleSort_F(TL * list, size_t startPos, size_t endPos)
{
	/*size_t max = sortAmount + startPos;

	for (size_t i = startPos; i < max; i++)
	{
		for (size_t j = i + 1; j < max; j++)
		{
			if (*(*list)[i] >= *(*list)[j])
			{
				TD* temp = (*list)[i];
				(*list)[i] = (*list)[j - 1];
				(*list)[j - 1] = temp;
				break;
			}
		}
	}*/

	for (size_t n = endPos; n > 1; n++)
	{
		for (i = 0; i < n - 1; i++) 
		{
			if (*(*list)[i] > *(*list)[i + 1]) 
			{
				TD* temp = (*list)[i];
				(*list)[i] = (*list)[i + 1];
				(*list)[i + 1] = temp;
			}
		}
	}
}

template<class TL, typename TD>//TL = Type list; TD = Type data
void MergeSort(TL * list, size_t startPos, size_t sortAmount)
{
	size_t lo = startPos;
	size_t hi = startPos + sortAmount;

	if (lo + 1 < hi)
	{
		size_t mid = (size_t)(lo + hi) / 2;
		std::thread* thread = new std::thread(&MergeSort<TL, TD>, list, lo, mid - lo);
		MergeSort<TL, TD>(list, mid, hi - mid);
		thread->join();

		//Merge lists
		{
			size_t i, j, k;
			size_t n1 = mid - lo + 1;
			size_t n2 = hi - mid;

			/* create temp arrays */
			TD** L = (TD**) malloc(n1 * sizeof(TD**));
			TD** R = (TD**) malloc(n2 * sizeof(TD**));

			/* Copy data to temp arrays L[] and R[] */
			for (i = 0; i < n1; i++)
				L[i] = (*list)[lo + i];
			for (j = 0; j < n2; j++)
				R[j] = (*list)[mid + 1 + j];

			/* Merge the temp arrays back into arr[l..r]*/
			i = 0; // Initial index of first subarray 
			j = 0; // Initial index of second subarray 
			k = lo; // Initial index of merged subarray 
			while (i < n1 && j < n2)
			{
				if (L[i] <= R[j])
				{
					(*list)[k] = L[i];
					i++;
				}
				else
				{
					(*list)[k] = R[j];
					j++;
				}
				k++;
			}

			/* Copy the remaining elements of L[], if there
			   are any */
			while (i < n1)
			{
				(*list)[k] = L[i];
				i++;
				k++;
			}

			/* Copy the remaining elements of R[], if there
			   are any */
			while (j < n2)
			{
				(*list)[k] = R[j];
				j++;
				k++;
			}

			free(L);
			free(R);
		}
	}
}